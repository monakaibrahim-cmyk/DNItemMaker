#include "MeshParser.h"
#include "Common/Logs/Logs.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>

namespace DNItemMaker::Graphics {

namespace {

constexpr const char* kMeshMagic = "Eternity Engine Mesh File";
constexpr size_t kHeaderSize = 1024;     // MESH_HEADER_RESERVED pads SMeshFileHeader to 1024 bytes
constexpr size_t kSubMeshHeaderSize = 1024; // SUBMESH_HEADER_RESERVED pads SSubMeshHeader to 1024 bytes
constexpr size_t kNameSize = 256;
constexpr size_t kMatrixSize = 64;       // EtMatrix (4x4 float)

/// Bounds-checked little-endian cursor over the file buffer.
class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    size_t Position() const { return m_pos; }
    bool Ok() const { return !m_failed; }

    bool Skip(size_t n) {
        if (n > m_size - m_pos) { m_failed = true; return false; }
        m_pos += n;
        return true;
    }

    template <typename T>
    T Read() {
        T v{};
        if (sizeof(T) > m_size - m_pos) { m_failed = true; return v; }
        std::memcpy(&v, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return v;
    }

    template <typename T>
    bool ReadArray(std::vector<T>& out, size_t count) {
        if (count > (m_size - m_pos) / sizeof(T)) { m_failed = true; return false; }
        out.resize(count);
        if (count) std::memcpy(out.data(), m_data + m_pos, count * sizeof(T));
        m_pos += count * sizeof(T);
        return true;
    }

    std::string ReadFixedString(size_t len) {
        if (len > m_size - m_pos) { m_failed = true; return {}; }
        const char* p = reinterpret_cast<const char*>(m_data + m_pos);
        m_pos += len;
        return std::string(p, strnlen(p, len));
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos = 0;
    bool m_failed = false;
};

/// Convert the engine's index buffer into a plain triangle list.
/// Strips use 0xFFFF as restart marker and alternate winding; degenerate faces are dropped.
void BuildTriangles(const std::vector<uint16_t>& idx, bool strip, std::vector<uint32_t>& out) {
    out.clear();
    if (!strip) {
        const size_t n = idx.size() / 3 * 3;
        out.assign(idx.begin(), idx.begin() + n);
        return;
    }

    if (idx.size() >= 3) {
        out.reserve((idx.size() - 2) * 3);
    }
    int run = 0;
    for (size_t i = 0; i + 2 < idx.size(); ++i) {
        if (idx[i] == 0xFFFF || idx[i + 1] == 0xFFFF || idx[i + 2] == 0xFFFF) {
            run = 0;
            continue;
        }
        uint32_t a = idx[i], b = idx[i + 1], c = idx[i + 2];
        if ((run & 1) != 0) std::swap(b, c);
        ++run;
        if (a == b || b == c || a == c) continue;
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
    }
}

} // namespace

std::string ParsedMesh::GetSummary() const {
    if (!valid) return std::format("Invalid mesh: {}", errorMessage);

    const auto ext = bbox.GetExtents();
    return std::format("Mesh: {} | Ver: {} | Submeshes: {} | Vertices: {} | Faces: {} | Bones: {} | BBox: [{:.1f} x {:.1f} x {:.1f}]",
                       filePath.filename().string(), version, submeshes.size(), GetTotalVertices(),
                       GetTotalTriangles(), bones.size(), ext.x, ext.y, ext.z);
}

ParsedMesh MeshParser::LoadFromFile(const std::filesystem::path& path) {
    ParsedMesh mesh;
    mesh.filePath = path;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        mesh.errorMessage = "Failed to open mesh file";
        LOG_WARNING(Channels::graphics, "Cannot open mesh: {}", path.string());
        return mesh;
    }

    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size))) {
        mesh.errorMessage = "Failed to read mesh file";
        return mesh;
    }

    ParsedMesh result = LoadFromMemory(buffer.data(), buffer.size(), path.string());
    result.filePath = path;
    return result;
}

ParsedMesh MeshParser::LoadFromMemory(const uint8_t* data, size_t size, const std::string& debugPath) {
    ParsedMesh mesh;

    if (size < kHeaderSize) {
        mesh.errorMessage = "File too small for mesh header";
        return mesh;
    }
    if (std::strncmp(reinterpret_cast<const char*>(data), kMeshMagic, std::strlen(kMeshMagic)) != 0) {
        mesh.errorMessage = "Not an Eternity Engine mesh file";
        return mesh;
    }

    Cursor cur(data, size);
    cur.Skip(kNameSize); // szHeaderString

    mesh.version = cur.Read<int32_t>();
    const int32_t subMeshCount = cur.Read<int32_t>();
    mesh.lodCount = cur.Read<int32_t>();
    mesh.uvAnimated = cur.Read<uint8_t>() != 0;
    cur.Skip(3); // cSpace

    // SMeshFileHeader stores MaxVec first, then MinVec.
    mesh.bbox.max = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };
    mesh.bbox.min = { cur.Read<float>(), cur.Read<float>(), cur.Read<float>() };

    const int32_t boneCount = cur.Read<int32_t>();
    mesh.collisionPrimitiveCount = cur.Read<int32_t>();
    mesh.dummyCount = cur.Read<int32_t>();

    if (!cur.Ok() || subMeshCount < 0 || subMeshCount > 4096 || boneCount < 0 || boneCount > 4096) {
        mesh.errorMessage = "Corrupt mesh header";
        return mesh;
    }

    // Header block is padded to 1024 bytes.
    const size_t headerRemaining = kHeaderSize - cur.Position();
    cur.Skip(headerRemaining);

    // Bones: 256-byte name + 4x4 inverse world matrix.
    mesh.bones.reserve(static_cast<size_t>(boneCount));
    for (int32_t i = 0; i < boneCount; ++i) {
        BoneNode bone;
        bone.name = cur.ReadFixedString(kNameSize);
        cur.Skip(kMatrixSize);
        mesh.bones.push_back(std::move(bone));
    }

    // Submeshes.
    mesh.submeshes.reserve(static_cast<size_t>(subMeshCount));
    std::vector<uint16_t> indices;
    for (int32_t s = 0; s < subMeshCount; ++s) {
        const size_t headerStart = cur.Position();

        Submesh sm;
        sm.parentName = cur.ReadFixedString(kNameSize);
        sm.name = cur.ReadFixedString(kNameSize);
        sm.vertexCount = cur.Read<int32_t>();
        sm.faceBufferSize = cur.Read<int32_t>();
        sm.textureChannels = cur.Read<int32_t>();
        sm.usesStrip = cur.Read<uint8_t>() != 0;
        sm.usesPhysique = cur.Read<uint8_t>() != 0;
        sm.usesVertexColor = cur.Read<uint8_t>() != 0;

        if (!cur.Ok() || sm.vertexCount < 0 || sm.faceBufferSize < 0 || sm.textureChannels < 0 ||
            sm.textureChannels > 8) {
            mesh.errorMessage = std::format("Corrupt header in submesh {}", s);
            return mesh;
        }

        // Skip the reserved padding up to the 1024-byte submesh header size.
        cur.Skip(kSubMeshHeaderSize - (cur.Position() - headerStart));

        cur.ReadArray(indices, static_cast<size_t>(sm.faceBufferSize));
        BuildTriangles(indices, sm.usesStrip, sm.triangles);

        const auto vc = static_cast<size_t>(sm.vertexCount);
        cur.ReadArray(sm.positions, vc);
        cur.ReadArray(sm.normals, vc);

        for (int32_t ch = 0; ch < sm.textureChannels; ++ch) {
            if (ch == 0) {
                cur.ReadArray(sm.uvs, vc);
            } else {
                cur.Skip(vc * sizeof(Vector2));
            }
        }
        if (sm.usesVertexColor) cur.Skip(vc * sizeof(uint32_t));
        if (sm.usesPhysique) {
            cur.Skip(vc * sizeof(int16_t) * 4); // bone indices
            cur.Skip(vc * sizeof(float) * 4);   // bone weights
            const int32_t linked = cur.Read<int32_t>();
            if (linked < 0 || linked > 4096) {
                mesh.errorMessage = std::format("Corrupt bone link count in submesh {}", s);
                return mesh;
            }
            sm.linkedBones.reserve(static_cast<size_t>(linked));
            for (int32_t b = 0; b < linked; ++b) sm.linkedBones.push_back(cur.ReadFixedString(kNameSize));
        }

        if (!cur.Ok()) {
            mesh.errorMessage = std::format("Unexpected end of file in submesh {}", s);
            return mesh;
        }

        // Out-of-range indices would crash a renderer later; reject early.
        for (uint32_t idx : sm.triangles) {
            if (idx >= vc) {
                mesh.errorMessage = std::format("Submesh {} references vertex {} of {}", s, idx, vc);
                return mesh;
            }
        }

        mesh.submeshes.push_back(std::move(sm));
    }

    mesh.valid = true;
    LOG_DEBUG(Channels::graphics,
              "Parsed mesh {} (v{}, {} submeshes, {} verts, {} tris, {} bones)",
              debugPath.empty() ? "<memory>" : debugPath, mesh.version, mesh.submeshes.size(),
              mesh.GetTotalVertices(), mesh.GetTotalTriangles(), mesh.bones.size());
    return mesh;
}

} // namespace DNItemMaker::Graphics
