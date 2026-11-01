#include "Graphics/ActParser.h"
#include "Engine/WorkspaceEngine.h"
#include <Common/Logs/Logs.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace DNItemMaker::Graphics {

namespace {
constexpr const char* kActMagic = "Eternity Action File";

inline bool EqualIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

class Cursor {
public:
    Cursor(const uint8_t* p, size_t n) : m_p(p), m_end(p + n) {}
    bool Ok() const noexcept { return m_ok && m_p <= m_end; }

    void Skip(size_t n) noexcept {
        if (m_p + n > m_end) { m_ok = false; return; }
        m_p += n;
    }

    template <typename T>
    T Read() noexcept {
        T val{};
        if (m_p + sizeof(T) > m_end) { m_ok = false; return val; }
        std::memcpy(&val, m_p, sizeof(T));
        m_p += sizeof(T);
        return val;
    }

    std::string ReadCString() {
        const int32_t len = Read<int32_t>();
        if (!m_ok || len <= 0 || m_p + len > m_end) {
            if (len < 0) m_ok = false;
            return {};
        }
        const char* s = reinterpret_cast<const char*>(m_p);
        size_t actualLen = 0;
        while (actualLen < static_cast<size_t>(len) && s[actualLen] != '\0') ++actualLen;
        m_p += len;
        return std::string(s, actualLen);
    }

private:
    const uint8_t* m_p = nullptr;
    const uint8_t* m_end = nullptr;
    bool m_ok = true;
};
} // namespace

int32_t ParsedActionFile::FindActionIndex(const std::string& actionName) const noexcept {
    for (size_t i = 0; i < actions.size(); ++i) {
        if (EqualIgnoreCase(actions[i].name, actionName)) return static_cast<int32_t>(i);
    }
    return -1;
}

const ActionEntry* ParsedActionFile::FindAction(const std::string& actionName) const noexcept {
    const int32_t idx = FindActionIndex(actionName);
    return idx >= 0 ? &actions[static_cast<size_t>(idx)] : nullptr;
}

ParsedActionFile ActParser::LoadFromFile(const std::filesystem::path& path) {
    ParsedActionFile act;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        act.errorMessage = "Failed to open action file";
        return act;
    }
    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    if (!file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size))) {
        act.errorMessage = "Failed to read action file";
        return act;
    }
    act = LoadFromMemory(buf.data(), buf.size(), path.parent_path(), path.string());
    act.filePath = path;
    return act;
}

ParsedActionFile ActParser::LoadFromMemory(const uint8_t* data, size_t size,
                                           const std::filesystem::path& basePath,
                                           const std::string& debugPath) {
    ParsedActionFile act;
    if (size < 32) {
        act.errorMessage = "File too small for action header";
        return act;
    }

    // Check if this is a binary action file
    if (std::memcmp(data, kActMagic, std::strlen(kActMagic)) == 0) {
        Cursor cur(data, size);
        cur.Skip(32); // Header magic string (32 bytes)

        act.version = cur.Read<int32_t>();
        const int32_t actionCount = cur.Read<int32_t>();

        if (!cur.Ok() || actionCount < 0 || actionCount > 65536) {
            act.errorMessage = "Corrupt action header counts";
            return act;
        }

        act.actions.reserve(static_cast<size_t>(actionCount));
        for (int32_t i = 0; i < actionCount; ++i) {
            ActionEntry entry;
            entry.name = cur.ReadCString();
            entry.linkAniName = cur.ReadCString();
            entry.length = cur.Read<uint32_t>();
            entry.nextActionName = cur.ReadCString();
            entry.blendFrame = cur.Read<uint32_t>();
            entry.nextActionFrame = cur.Read<uint32_t>();

            if (act.version >= 2) cur.Skip(4); // dwUnkAct2
            if (act.version >= 3) cur.Skip(4); // dwUnkAct3
            if (act.version >= 4) cur.Skip(1); // bUnkAct4
            if (act.version >= 5) cur.Skip(4); // dwUnkAct5

            bool bUnkAct6 = false;
            if (act.version >= 6) {
                bUnkAct6 = cur.Read<uint8_t>() != 0;
            }

            const int32_t sigCount = cur.Read<int32_t>();
            if (sigCount > 0 && sigCount < 65536) {
                for (int32_t s = 0; s < sigCount; ++s) {
                    cur.Skip(16);  // sigIndex, startFrame, endFrame, yOrder
                    cur.Skip(256); // signal buffer (64 ints)

                    const int32_t v2Count = cur.Read<int32_t>();
                    if (v2Count > 0) {
                        cur.Skip(static_cast<size_t>(v2Count) * 4); // table indices
                        cur.Skip(static_cast<size_t>(v2Count) * 8); // EtVector2
                    }

                    const int32_t v3Count = cur.Read<int32_t>();
                    if (v3Count > 0) {
                        cur.Skip(static_cast<size_t>(v3Count) * 4);
                        cur.Skip(static_cast<size_t>(v3Count) * 12); // EtVector3
                    }

                    const int32_t v4Count = cur.Read<int32_t>();
                    if (v4Count > 0) {
                        cur.Skip(static_cast<size_t>(v4Count) * 4);
                        cur.Skip(static_cast<size_t>(v4Count) * 16); // EtVector4
                    }

                    const int32_t strCount = cur.Read<int32_t>();
                    if (strCount > 0) {
                        cur.Skip(static_cast<size_t>(strCount) * 4);
                        for (int32_t st = 0; st < strCount; ++st) {
                            cur.ReadCString();
                        }
                    }

                    if (bUnkAct6) {
                        cur.Skip(1); // v6 trailer byte
                    }
                }
            }

            if (!cur.Ok()) {
                act.errorMessage = "Premature EOF in action entries";
                return act;
            }

            act.actions.push_back(std::move(entry));
        }

        act.valid = true;
        LOG_DEBUG(Channels::graphics, "Parsed binary action file {} (v{}, {} actions)",
                  debugPath.empty() ? "<memory>" : debugPath, act.version, act.actions.size());
        return act;
    }

    // Otherwise, treat as a text manifest listing action filenames
    std::string text(reinterpret_cast<const char*>(data), size);
    std::istringstream stream(text);
    std::string line;
    std::vector<std::string> subActNames;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
        if (start < line.size() && line[start] != '#' && line[start] != ';') {
            subActNames.push_back(line.substr(start));
        }
    }

    if (subActNames.empty()) {
        act.errorMessage = "Manifest action file is empty";
        return act;
    }

    // Try loading actions from listed sub-act files in the same directory
    for (const auto& subName : subActNames) {
        std::filesystem::path subPath = basePath / subName;
        if (!std::filesystem::exists(subPath)) {
            // Case-insensitive fallback lookup in directory
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(basePath, ec)) {
                if (EqualIgnoreCase(entry.path().filename().string(), subName)) {
                    subPath = entry.path();
                    break;
                }
            }
        }

        if (std::filesystem::exists(subPath)) {
            ParsedActionFile subAct = LoadFromFile(subPath);
            if (subAct.valid) {
                for (auto& a : subAct.actions) {
                    if (act.FindActionIndex(a.name) < 0) {
                        act.actions.push_back(std::move(a));
                    }
                }
            }
        }
    }

    if (!act.actions.empty()) {
        act.valid = true;
        act.version = 6;
        LOG_DEBUG(Channels::graphics, "Parsed manifest action file {} ({} actions loaded)",
                  debugPath.empty() ? "<manifest>" : debugPath, act.actions.size());
    } else {
        act.errorMessage = "Could not load sub-action files referenced in manifest";
    }

    return act;
}

} // namespace DNItemMaker::Graphics
