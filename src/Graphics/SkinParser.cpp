#include "SkinParser.h"

#include "Common/Logs/Logs.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>

namespace DNItemMaker::Graphics {

namespace {

constexpr size_t kBlockSize = 1024;   // SSkinFileHeader / SSubSkinHeader are both padded to 1024 bytes.
constexpr size_t kNameSize = 256;
constexpr const char* kSkinMagic = "Eternity Engine Skin File";
constexpr int32_t kMaxNameLength = 4096;

enum ParamType : int32_t { PT_INT = 0, PT_FLOAT = 1, PT_FLOAT4 = 2, PT_TEX = 3 };

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    bool Has(size_t n) const { return m_pos <= m_size && m_size - m_pos >= n; }
    size_t Position() const { return m_pos; }
    void Seek(size_t pos) { m_pos = pos; }

    bool ReadInt(int32_t& out) {
        if (!Has(4)) return false;
        std::memcpy(&out, m_data + m_pos, 4);
        m_pos += 4;
        return true;
    }
    bool ReadFloats(float* out, size_t count) {
        if (!Has(count * 4)) return false;
        std::memcpy(out, m_data + m_pos, count * 4);
        m_pos += count * 4;
        return true;
    }
    /// Length-prefixed string as written by CSaveSkin::WriteString (length includes the terminator).
    bool ReadString(std::string& out) {
        int32_t len = 0;
        if (!ReadInt(len) || len < 0 || len > kMaxNameLength || !Has(static_cast<size_t>(len))) return false;
        const char* s = reinterpret_cast<const char*>(m_data + m_pos);
        out.assign(s, strnlen(s, static_cast<size_t>(len)));
        m_pos += static_cast<size_t>(len);
        return true;
    }
    std::string FixedString(size_t offset, size_t maxLen) const {
        if (offset >= m_size) return {};
        const char* s = reinterpret_cast<const char*>(m_data + offset);
        return std::string(s, strnlen(s, std::min(maxLen, m_size - offset)));
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos = 0;
};

constexpr char AsciiLower(char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    return static_cast<char>((u >= 'A' && u <= 'Z') ? (u + ('a' - 'A')) : u);
}

bool IEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (AsciiLower(a[i]) != AsciiLower(b[i])) return false;
    }
    return true;
}

} // namespace

ParsedSkin SkinParser::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ParsedSkin skin;
        skin.errorMessage = "Cannot open file";
        skin.filePath = path;
        return skin;
    }
    const auto endPos = file.tellg();
    if (endPos <= 0) {
        ParsedSkin skin;
        skin.errorMessage = "Empty file";
        skin.filePath = path;
        return skin;
    }
    const auto size = static_cast<size_t>(endPos);
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));

    ParsedSkin skin = LoadFromMemory(buffer.data(), buffer.size());
    skin.filePath = path;
    if (skin.valid && !skin.complete) {
        LOG_WARNING(Channels::graphics,
                    "Skin {} is partially corrupt ({}); using {} of its material slots",
                    path.filename().string(), skin.errorMessage, skin.subSkins.size());
    }
    return skin;
}

ParsedSkin SkinParser::LoadFromMemory(const uint8_t* data, size_t size) {
    ParsedSkin skin;
    if (size < kBlockSize || std::strncmp(reinterpret_cast<const char*>(data), kSkinMagic, std::strlen(kSkinMagic)) != 0) {
        skin.errorMessage = "Not an Eternity Engine skin file";
        return skin;
    }

    Reader r(data, size);
    skin.meshName = r.FixedString(kNameSize, kNameSize);
    r.Seek(2 * kNameSize);
    int32_t subCount = 0;
    r.ReadInt(skin.version);
    r.ReadInt(subCount);
    if (skin.meshName.empty() || subCount < 0 || subCount > 4096) {
        skin.errorMessage = "Corrupt skin header";
        return skin;
    }
    skin.valid = true;
    skin.subSkins.reserve(static_cast<size_t>(subCount));

    r.Seek(kBlockSize);
    for (int32_t i = 0; i < subCount; ++i) {
        const size_t start = r.Position();
        if (!r.Has(kBlockSize)) {
            skin.errorMessage = std::format("truncated at material slot {}", i);
            return skin;
        }

        SubSkin sub;
        sub.materialName = r.FixedString(start, kNameSize);
        sub.effectName = r.FixedString(start + kNameSize, kNameSize);
        std::memcpy(&sub.alphaValue, data + start + 2 * kNameSize, sizeof(float));
        sub.alphaBlend = data[start + 2 * kNameSize + 4] != 0;
        r.Seek(start + kBlockSize);

        int32_t paramCount = 0;
        if (!r.ReadInt(paramCount) || paramCount < 0 || paramCount > 1024) {
            skin.errorMessage = std::format("bad parameter count in slot {}", i);
            return skin;
        }

        for (int32_t p = 0; p < paramCount; ++p) {
            std::string name;
            int32_t type = 0;
            if (!r.ReadString(name) || !r.ReadInt(type)) {
                skin.errorMessage = std::format("bad parameter {} in slot {}", p, i);
                return skin;
            }
            switch (type) {
                case PT_INT: {
                    int32_t dummy = 0;
                    if (!r.ReadInt(dummy)) {
                        skin.errorMessage = std::format("truncated parameter '{}'", name);
                        return skin;
                    }
                    break;
                }
                case PT_FLOAT: {
                    float fVal = 0.0f;
                    if (!r.ReadFloats(&fVal, 1)) {
                        skin.errorMessage = std::format("truncated parameter '{}'", name);
                        return skin;
                    }
                    if (IEquals(name, "g_EmissivePower")) sub.emissivePower = fVal;
                    break;
                }
                case PT_FLOAT4: {
                    std::array<float, 4> v{};
                    if (!r.ReadFloats(v.data(), 4)) {
                        skin.errorMessage = std::format("truncated parameter '{}'", name);
                        return skin;
                    }
                    if (IEquals(name, "g_MaterialDiffuse")) sub.diffuseColor = v;
                    break;
                }
                case PT_TEX: {
                    std::string tex;
                    if (!r.ReadString(tex)) {
                        skin.errorMessage = std::format("truncated texture parameter '{}'", name);
                        return skin;
                    }
                    if (IEquals(name, "g_DiffuseTex") || IEquals(name, "g_DiffuseVolumeTex")) {
                        sub.diffuseTexture = tex;
                    } else if (IEquals(name, "g_EmissiveTex")) {
                        sub.emissiveTexture = tex;
                    }
                    break;
                }
                default:
                    skin.errorMessage = std::format("unknown parameter type {} for '{}'", type, name);
                    return skin;
            }
        }
        skin.subSkins.push_back(std::move(sub));
    }

    skin.complete = true;
    return skin;
}

} // namespace DNItemMaker::Graphics
