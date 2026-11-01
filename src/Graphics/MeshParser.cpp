#include "Graphics/MeshParser.h"
#include "Engine/WorkspaceEngine.h"
#include <Common/Logs/Logs.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>

namespace DNItemMaker::Graphics {

namespace {

constexpr const char* kMeshMagic = "Eternity Engine Mesh File";
constexpr size_t kHeaderSize = 1024;
constexpr size_t kSubMeshHeaderSize = 1024;
constexpr size_t kNameSize = 256;
constexpr size_t kMatrixSize = 64; // 4x4 float

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    bool Ok() const noexcept { return m_offset <= m_size && !m_failed; }
    size_t Position() const noexcept { return m_offset; }
    size_t Remaining() const noexcept { return m_offset <= m_size ? m_size - m_offset : 0; }

    void Skip(size_t bytes) noexcept {
        if (m_offset + bytes > m_size) {
            m_failed = true;
            m_offset = m_size;
            return;
        }
        m_offset += bytes;
    }

    template <typename T>
    T Read() noexcept {
        T value{};
        if (m_offset + sizeof(T) > m_size) {
            m_failed = true;
            return value;
        }
        std::memcpy(&value, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return value;
    }

    template <typename T>
    void ReadArray(std::vector<T>& out, size_t count) {
        out.resize(count);
        const size_t byteCount = count * sizeof(T);
        if (m_offset + byteCount > m_size) {
            m_failed = true;
            out.clear();
            return;
        }
        std::memcpy(out.data(), m_data + m_offset, byteCount);
        m_offset += byteCount;
    }

    std::string ReadFixedString(size_t maxLen) {
        if (m_offset + maxLen > m_size) {
            m_failed = true;
            return {};
        }
        const char* p = reinterpret_cast<const char*>(m_data + m_offset);
        size_t len = 0;
        while (len < maxLen && p[len] != '\0') ++len;
        m_offset += maxLen;
        return std::string(p, len);
    }

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_offset = 0;
    bool m_failed = false;
};

void BuildTriangles(const std::vector<uint16_t>& indices, bool isStrip, std::vector<uint32_t>& out) {
    out.clear();
    if (indices.size() < 3) return;

    if (!isStrip) {
        out.reserve(indices.size());
        for (uint16_t idx : indices) out.push_back(idx);
        return;
    }

    out.reserve((indices.size() - 2) * 3);
    for (size_t i = 0; i + 2 < indices.size(); ++i) {
        const uint16_t a = indices[i];
        const uint16_t b = indices[i + 1];
        const uint16_t c = indices[i + 2];
        if (a == b || b == c || a == c) continue; // degenerate
        if ((i & 1) == 0) {
            out.push_back(a); out.push_back(b); out.push_back(c);
        } else {
            out.push_back(b); out.push_back(a); out.push_back(c);
        }
    }
}

} // namespace

std::string ParsedMesh::GetSummary() const {
    if (!valid) return "Invalid mesh: " + errorMessage;
    const auto ext = bbox.GetExtents();
    return std::format("v{} (LODs: {}) - {} submeshes, {} vertices, {} triangles, {} bones, {} dummies - Box: [{:.2f}, {:.2f}, {:.2f}]",
                       version, lodCount, submeshes.size(), GetTotalVertices(), GetTotalTriangles(), bones.size(), dummies.size(),
                       ext.x, ext.y, ext.z);
}

ParsedMesh MeshParser::LoadFromFile(const std::filesystem::path& path) {
    ParsedMesh mesh;
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
        for (int k = 0; k < 16; ++k) {
            bone.invWorldMatrix[k] = cur.Read<float>();
        }
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
            sm.skinWeights.resize(vc);
            for (size_t i = 0; i < vc; ++i) {
                sm.skinWeights[i].boneIndices[0] = static_cast<uint16_t>(cur.Read<int16_t>());
                sm.skinWeights[i].boneIndices[1] = static_cast<uint16_t>(cur.Read<int16_t>());
                sm.skinWeights[i].boneIndices[2] = static_cast<uint16_t>(cur.Read<int16_t>());
                sm.skinWeights[i].boneIndices[3] = static_cast<uint16_t>(cur.Read<int16_t>());
            }
            for (size_t i = 0; i < vc; ++i) {
                sm.skinWeights[i].weights[0] = cur.Read<float>();
                sm.skinWeights[i].weights[1] = cur.Read<float>();
                sm.skinWeights[i].weights[2] = cur.Read<float>();
                sm.skinWeights[i].weights[3] = cur.Read<float>();
            }

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

    // Skip collision primitives if present
    for (int32_t i = 0; i < mesh.collisionPrimitiveCount; ++i) {
        const int32_t ctype = cur.Read<int32_t>();
        if (mesh.version > 10) {
            const int32_t strLen = cur.Read<int32_t>();
            if (strLen > 0) cur.Skip(static_cast<size_t>(strLen));
        }
        if (ctype == 0) {
            cur.Skip(12 + 36); // Box: origin + 3 axes
        } else if (ctype == 1) {
            cur.Skip(12 + 4);  // Sphere: origin + radius
        } else if (ctype == 2) {
            cur.Skip(12 + 12 + 4); // Capsule: origin + dir + radius
        } else if (ctype == 3 || ctype == 4) {
            const int32_t triCount = cur.Read<int32_t>();
            if (triCount > 0) cur.Skip(static_cast<size_t>(triCount) * 36);
        }
    }

    // Read attachment dummy nodes
    if (mesh.dummyCount > 0 && mesh.dummyCount < 4096) {
        mesh.dummies.reserve(static_cast<size_t>(mesh.dummyCount));
        if (mesh.version >= 13) {
            for (int32_t i = 0; i < mesh.dummyCount; ++i) {
                DummyNode d;
                d.name = cur.ReadFixedString(kNameSize);
                d.parentName = cur.ReadFixedString(kNameSize);
                for (int k = 0; k < 16; ++k) d.matrix[k] = cur.Read<float>();
                mesh.dummies.push_back(std::move(d));
            }
        } else {
            for (int32_t i = 0; i < mesh.dummyCount; ++i) {
                DummyNode d;
                std::string rawName = cur.ReadFixedString(kNameSize);
                if (!rawName.empty() && rawName[0] == 'L') {
                    d.name = rawName.substr(1);
                    d.matrix[12] = cur.Read<float>();
                    d.matrix[13] = cur.Read<float>();
                    d.matrix[14] = cur.Read<float>();
                    d.parentName = cur.ReadFixedString(kNameSize);
                } else {
                    d.name = rawName;
                    d.parentName = cur.ReadFixedString(kNameSize);
                    for (int k = 0; k < 16; ++k) d.matrix[k] = cur.Read<float>();
                }
                mesh.dummies.push_back(std::move(d));
            }
        }
    }

    mesh.valid = true;
    LOG_DEBUG(Channels::graphics,
              "Parsed mesh {} (v{}, {} submeshes, {} verts, {} tris, {} bones, {} dummies)",
              debugPath.empty() ? "<memory>" : debugPath, mesh.version, mesh.submeshes.size(),
              mesh.GetTotalVertices(), mesh.GetTotalTriangles(), mesh.bones.size(), mesh.dummies.size());
    return mesh;
}

} // namespace DNItemMaker::Graphics
