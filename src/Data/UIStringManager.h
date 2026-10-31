#pragma once

/**
 * @file UIStringManager.h
 * @brief High-speed memory-arena parser and indexed lookup engine for uistring.xml.
 *
 * Implements a memory arena with an open-addressed power-of-two hash index for
 * O(1) Message ID (MID) lookups, zero-copy string views, live text overriding,
 * and byte-exact XML serialization preserving original header tags.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DNItemMaker::Data {

/**
 * @brief Search match entry containing Message ID and matched text snippet.
 */
struct UIStringHit {
    uint32_t mid = 0;   ///< Message ID (e.g. 100001).
    std::string text;   ///< Localized text content associated with this Message ID.
};

/**
 * @class UIStringManager
 * @brief In-memory manager for loading, searching, updating, and serializing `uistring.xml`.
 *
 * Employs a single contiguous buffer (arena) for XML parsing and an open-addressing
 * hash table indexing all `<message mid="...">` nodes for sub-microsecond retrieval.
 */
class UIStringManager {
public:
    UIStringManager() = default;

    /**
     * @brief Parses and indexes `uistring.xml` from the specified file path.
     * @param path Filesystem path to `uistring.xml`.
     * @return true on successful parsing and indexing, false on I/O or syntax error.
     */
    bool LoadFromFile(const std::filesystem::path& path);

    /**
     * @brief Serializes all base and overridden string entries back to XML format.
     * @param path Target filesystem path.
     * @param quiet If true, suppresses non-error logging output.
     * @return true if written successfully, false on I/O error.
     */
    bool SaveToFile(const std::filesystem::path& path, bool quiet = false) const;

    /**
     * @brief Checks if a Message ID exists in either the base index or override map.
     * @param mid Message ID to query.
     * @return true if defined, false otherwise.
     */
    bool HasString(uint32_t mid) const noexcept;

    /**
     * @brief Retrieves the string content for a Message ID, returning default if absent.
     * @param mid Message ID to query.
     * @param def Default string returned if mid is not found.
     * @return Resolved string value.
     */
    std::string GetString(uint32_t mid, const std::string& def = "") const;

    /**
     * @brief Retrieves a zero-copy string view for a Message ID.
     * @param mid Message ID to query.
     * @return std::string_view pointing directly into the arena or override buffer.
     */
    std::string_view GetStringView(uint32_t mid) const noexcept;

    /**
     * @brief Sets or overrides the text content for a Message ID.
     * @param mid Target Message ID.
     * @param text New text content.
     */
    void SetString(uint32_t mid, std::string_view text);

    /**
     * @brief Performs case-insensitive substring search across all loaded strings.
     * @param query Search query substring.
     * @param maxResults Maximum number of matches to return.
     * @return List of matching UIStringHit records.
     */
    std::vector<UIStringHit> SearchStrings(std::string_view query, size_t maxResults = 250) const;

    /** @brief Allocates and reserves the next available Message ID. */
    uint32_t AllocateNextMid() noexcept { return ++m_maxMid; }

    /** @brief Returns the highest Message ID observed across base and override pools. */
    uint32_t GetMaxMid() const noexcept { return m_maxMid; }

    /** @brief Returns the total count of registered string entries. */
    size_t GetCount() const noexcept { return m_entries.size() + m_newOverrideCount; }

    /** @brief Returns the path of the loaded `uistring.xml` file. */
    const std::filesystem::path& GetPath() const noexcept { return m_filePath; }

private:
    struct Entry {
        uint32_t mid = 0;
        uint32_t offset = 0;
        uint32_t length = 0;
    };

    const Entry* FindBaseEntry(uint32_t mid) const noexcept;
    void RebuildHashIndex();

    std::filesystem::path m_filePath;
    std::string m_arena;
    std::string m_header;
    std::vector<Entry> m_entries;
    std::vector<uint32_t> m_hashTable;
    std::unordered_map<uint32_t, std::string> m_overrides;
    size_t m_newOverrideCount = 0;
    uint32_t m_maxMid = 0;
};

} // namespace DNItemMaker::Data
