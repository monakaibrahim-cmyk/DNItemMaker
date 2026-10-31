#include "UIStringManager.h"
#include "Common/Logs/Logs.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>

namespace DNItemMaker::Data {

namespace {

constexpr uint32_t HashMid(uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

const char* FindLiteral(const char* cur, const char* end, std::string_view needle) noexcept {
    if (needle.empty()) return cur;
    const char first = needle.front();
    const size_t nLen = needle.size();
    while (static_cast<size_t>(end - cur) >= nLen) {
        const auto* hit = static_cast<const char*>(std::memchr(cur, static_cast<unsigned char>(first),
                                                               static_cast<size_t>(end - cur) - nLen + 1));
        if (!hit) return nullptr;
        if (std::memcmp(hit, needle.data(), nLen) == 0) return hit;
        cur = hit + 1;
    }
    return nullptr;
}

} // namespace

void UIStringManager::RebuildHashIndex() {
    m_hashTable.clear();
    if (m_entries.empty()) return;

    size_t cap = 1024;
    while (cap < m_entries.size() * 2) {
        cap <<= 1;
    }
    m_hashTable.assign(cap, 0u);
    const size_t mask = cap - 1;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        size_t slot = HashMid(m_entries[i].mid) & mask;
        while (m_hashTable[slot] != 0u) {
            slot = (slot + 1) & mask;
        }
        m_hashTable[slot] = static_cast<uint32_t>(i + 1);
    }
}

const UIStringManager::Entry* UIStringManager::FindBaseEntry(uint32_t mid) const noexcept {
    if (m_hashTable.empty()) return nullptr;
    const size_t mask = m_hashTable.size() - 1;
    size_t slot = HashMid(mid) & mask;
    while (const uint32_t idx1 = m_hashTable[slot]) {
        const Entry& e = m_entries[idx1 - 1];
        if (e.mid == mid) return &e;
        slot = (slot + 1) & mask;
    }
    return nullptr;
}

bool UIStringManager::LoadFromFile(const std::filesystem::path& path) {
    m_filePath = path;
    m_arena.clear();
    m_header.clear();
    m_entries.clear();
    m_hashTable.clear();
    m_overrides.clear();
    m_newOverrideCount = 0;
    m_maxMid = 0;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR(Channels::dnt, "Failed to open uistring file: {}", path.string());
        return false;
    }

    const std::streamoff fileSizeSigned = file.tellg();
    if (fileSizeSigned < 0) {
        LOG_ERROR(Channels::dnt, "Failed to stat uistring file: {}", path.string());
        return false;
    }
    const auto fileSize = static_cast<size_t>(fileSizeSigned);
    file.seekg(0, std::ios::beg);

    m_arena.resize(fileSize);
    if (fileSize > 0 && !file.read(m_arena.data(), static_cast<std::streamsize>(fileSize))) {
        LOG_ERROR(Channels::dnt, "Failed to read uistring buffer: {}", path.string());
        return false;
    }

    constexpr std::string_view kTagStart = "<message mid=\"";
    constexpr std::string_view kCdataStart = "<![CDATA[";
    constexpr std::string_view kCdataEnd = "]]>";
    constexpr std::string_view kTagEnd = "</message>";

    const char* const base = m_arena.data();
    const char* const end = base + m_arena.size();

    const char* firstTag = FindLiteral(base, end, kTagStart);
    if (firstTag) {
        m_header.assign(base, static_cast<size_t>(firstTag - base));
    }

    m_entries.reserve(fileSize / 80 + 1024);

    bool strictlyAscending = true;
    uint32_t prevMid = 0;
    bool hasPrev = false;

    const char* cur = firstTag ? firstTag : end;
    while (cur < end) {
        const char* tagPos = FindLiteral(cur, end, kTagStart);
        if (!tagPos) break;

        const char* midStart = tagPos + kTagStart.size();
        const auto* midEnd = static_cast<const char*>(
            std::memchr(midStart, '"', static_cast<size_t>(end - midStart)));
        if (!midEnd) break;

        uint32_t mid = 0;
        const auto [ptr, ec] = std::from_chars(midStart, midEnd, mid);
        if (ec != std::errc{}) {
            cur = midEnd + 1;
            continue;
        }

        if (mid > m_maxMid) m_maxMid = mid;

        const auto* gtPos = static_cast<const char*>(
            std::memchr(midEnd + 1, '>', static_cast<size_t>(end - (midEnd + 1))));
        if (!gtPos) break;
        const char* bodyStart = gtPos + 1;

        const char* msgClose = FindLiteral(bodyStart, end, kTagEnd);
        if (!msgClose) break;

        const char* textPtr = bodyStart;
        size_t textLen = static_cast<size_t>(msgClose - bodyStart);

        const char* cdStart = FindLiteral(bodyStart, msgClose, kCdataStart);
        if (cdStart) {
            const char* innerStart = cdStart + kCdataStart.size();
            const char* cdEnd = FindLiteral(innerStart, msgClose, kCdataEnd);
            textPtr = innerStart;
            textLen = static_cast<size_t>((cdEnd ? cdEnd : msgClose) - innerStart);
        }

        if (hasPrev && mid <= prevMid) {
            strictlyAscending = false;
        }
        prevMid = mid;
        hasPrev = true;

        m_entries.push_back(Entry{
            mid,
            static_cast<uint32_t>(textPtr - base),
            static_cast<uint32_t>(textLen)
        });

        cur = msgClose + kTagEnd.size();
    }

    if (!strictlyAscending && m_entries.size() > 1) {
        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const Entry& a, const Entry& b) noexcept { return a.mid < b.mid; });
        m_entries.erase(std::unique(m_entries.begin(), m_entries.end(),
                                    [](const Entry& a, const Entry& b) noexcept { return a.mid == b.mid; }),
                        m_entries.end());
    }

    RebuildHashIndex();

    const auto u8Name = path.filename().u8string();
    const std::string fileName(reinterpret_cast<const char*>(u8Name.data()), u8Name.size());
    LOG_INFO(Channels::dnt, "UIString loaded: {} strings parsed (Max MID: {}) from '{}'",
             m_entries.size(), m_maxMid, fileName);
    return true;
}

bool UIStringManager::SaveToFile(const std::filesystem::path& path, bool quiet) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        LOG_ERROR(Channels::dnt, "Failed to open uistring for writing: {}", path.string());
        return false;
    }

    std::string out;
    out.reserve(m_arena.empty() ? (GetCount() * 110 + 256) : (m_arena.size() + m_overrides.size() * 128 + 512));

    if (m_header.empty()) {
        out.append("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<messages name=\"UIString\" lang=\"SN1\">\r\n");
    } else {
        out.append(m_header);
    }

    char numBuf[16];
    auto writeRecord = [&](uint32_t mid, std::string_view text) {
        out.append("<message mid=\"");
        const auto [p, ec] = std::to_chars(numBuf, numBuf + sizeof(numBuf), mid);
        out.append(numBuf, static_cast<size_t>(p - numBuf));
        out.append("\"><![CDATA[");
        out.append(text);
        out.append("]]></message>\r\n");
    };

    if (m_overrides.empty()) {
        const char* const base = m_arena.data();
        for (const Entry& e : m_entries) {
            writeRecord(e.mid, std::string_view(base + e.offset, e.length));
        }
    } else {
        // Collect only brand-new MIDs (not present in m_entries) and merge in sorted order.
        std::vector<uint32_t> extraMids;
        extraMids.reserve(m_newOverrideCount);
        for (const auto& [mid, _] : m_overrides) {
            if (!FindBaseEntry(mid)) {
                extraMids.push_back(mid);
            }
        }
        std::sort(extraMids.begin(), extraMids.end());

        const char* const base = m_arena.data();
        size_t i = 0;
        size_t j = 0;
        while (i < m_entries.size() || j < extraMids.size()) {
            if (j >= extraMids.size() || (i < m_entries.size() && m_entries[i].mid < extraMids[j])) {
                const Entry& e = m_entries[i++];
                const auto ov = m_overrides.find(e.mid);
                const std::string_view text = (ov != m_overrides.end())
                                                  ? std::string_view(ov->second)
                                                  : std::string_view(base + e.offset, e.length);
                writeRecord(e.mid, text);
            } else {
                const uint32_t mid = extraMids[j++];
                writeRecord(mid, m_overrides.at(mid));
            }
        }
    }

    out.append("</messages>");

    file.write(out.data(), static_cast<std::streamsize>(out.size()));
    file.flush();
    if (!file.good()) {
        LOG_ERROR(Channels::dnt, "I/O error while writing uistring '{}'", path.string());
        return false;
    }

    if (!quiet) {
        const auto u8Name = path.filename().u8string();
        const std::string fileName(reinterpret_cast<const char*>(u8Name.data()), u8Name.size());
        LOG_INFO(Channels::dnt, "Saved uistring '{}' with {} strings", fileName, GetCount());
    }
    return true;
}

bool UIStringManager::HasString(uint32_t mid) const noexcept {
    if (!m_overrides.empty() && m_overrides.contains(mid)) return true;
    return FindBaseEntry(mid) != nullptr;
}

std::string UIStringManager::GetString(uint32_t mid, const std::string& def) const {
    if (!m_overrides.empty()) {
        const auto it = m_overrides.find(mid);
        if (it != m_overrides.end()) return it->second;
    }
    if (const Entry* e = FindBaseEntry(mid)) {
        return std::string(m_arena.data() + e->offset, e->length);
    }
    return def;
}

std::string_view UIStringManager::GetStringView(uint32_t mid) const noexcept {
    if (!m_overrides.empty()) {
        const auto it = m_overrides.find(mid);
        if (it != m_overrides.end()) return it->second;
    }
    if (const Entry* e = FindBaseEntry(mid)) {
        return std::string_view(m_arena.data() + e->offset, e->length);
    }
    return {};
}

void UIStringManager::SetString(uint32_t mid, std::string_view text) {
    const auto [it, inserted] = m_overrides.try_emplace(mid, text);
    if (!inserted) {
        it->second.assign(text.data(), text.size());
    } else if (!FindBaseEntry(mid)) {
        ++m_newOverrideCount;
    }
    if (mid > m_maxMid) m_maxMid = mid;
}

std::vector<UIStringHit> UIStringManager::SearchStrings(std::string_view query, size_t maxResults) const {
    std::vector<UIStringHit> hits;
    if (maxResults == 0) return hits;

    while (!query.empty() && static_cast<unsigned char>(query.front()) <= ' ') query.remove_prefix(1);
    while (!query.empty() && static_cast<unsigned char>(query.back()) <= ' ') query.remove_suffix(1);
    if (!query.empty() && query.front() == '#') query.remove_prefix(1);

    auto asciiLower = [](char c) noexcept -> char {
        const auto u = static_cast<unsigned char>(c);
        return static_cast<char>((u >= 'A' && u <= 'Z') ? (u + ('a' - 'A')) : u);
    };

    std::string needle(query.size(), '\0');
    bool allDigits = !query.empty();
    for (size_t i = 0; i < query.size(); ++i) {
        needle[i] = asciiLower(query[i]);
        if (query[i] < '0' || query[i] > '9') allDigits = false;
    }

    hits.reserve(std::min<size_t>(maxResults, 256));

    uint32_t exactMid = 0;
    bool hasExactMid = false;
    if (allDigits) {
        const auto [p, ec] = std::from_chars(query.data(), query.data() + query.size(), exactMid);
        if (ec == std::errc{} && p == query.data() + query.size() && HasString(exactMid)) {
            hasExactMid = true;
            hits.push_back(UIStringHit{ exactMid, GetString(exactMid) });
            if (hits.size() >= maxResults) return hits;
        }
    }

    auto matchesText = [&](std::string_view text) noexcept -> bool {
        if (needle.empty()) return !text.empty();
        if (text.size() < needle.size()) return false;
        const size_t limit = text.size() - needle.size();
        const char n0 = needle.front();
        for (size_t i = 0; i <= limit; ++i) {
            if (asciiLower(text[i]) != n0) continue;
            size_t j = 1;
            while (j < needle.size() && asciiLower(text[i + j]) == needle[j]) ++j;
            if (j == needle.size()) return true;
        }
        return false;
    };

    // Search new runtime overrides
    for (const auto& [mid, val] : m_overrides) {
        if (hasExactMid && mid == exactMid) continue;
        if (!FindBaseEntry(mid) && matchesText(val)) {
            hits.push_back(UIStringHit{ mid, val });
            if (hits.size() >= maxResults) return hits;
        }
    }

    // Search base arena entries
    const char* const base = m_arena.data();
    char midBuf[16];
    for (const Entry& e : m_entries) {
        if (hasExactMid && e.mid == exactMid) continue;
        const auto ov = m_overrides.empty() ? m_overrides.end() : m_overrides.find(e.mid);
        const std::string_view sv = (ov != m_overrides.end())
                                        ? std::string_view(ov->second)
                                        : std::string_view(base + e.offset, e.length);
        bool ok = matchesText(sv);
        if (!ok && allDigits && needle.size() >= 3) {
            const auto [p, ec] = std::to_chars(midBuf, midBuf + sizeof(midBuf), e.mid);
            if (ec == std::errc{}) {
                ok = std::string_view(midBuf, static_cast<size_t>(p - midBuf)).find(needle) != std::string_view::npos;
            }
        }
        if (ok) {
            hits.push_back(UIStringHit{ e.mid, std::string(sv) });
            if (hits.size() >= maxResults) break;
        }
    }
    return hits;
}

} // namespace DNItemMaker::Data
