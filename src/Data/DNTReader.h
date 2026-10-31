#pragma once

/**
 * @file DNTReader.h
 * @brief High-performance Dragon Nest binary table (.dnt) parser and serializer.
 *
 * Implements high-speed zero-copy memory slab allocations and column-oriented
 * contiguous storage for reading, modifying, and serializing Dragon Nest .dnt files
 * with 1:1 byte-exact fidelity.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DNItemMaker::Data {

/**
 * @brief Binary data type identifiers as encoded in Dragon Nest .dnt column headers.
 */
enum class DNTFieldType : uint8_t {
    NA     = 0, ///< Unknown or unassigned field type.
    STRING = 1, ///< Null-terminated or length-prefixed ASCII/UTF-8 string.
    BOOL   = 2, ///< 1-byte boolean flag (0 = false, 1 = true).
    INT    = 3, ///< 32-bit signed integer (int32_t).
    PER    = 4, ///< 32-bit floating point or integer representation of percentage.
    FLOAT  = 5, ///< 32-bit IEEE 754 single-precision float.
    INT64  = 6  ///< 64-bit signed integer (int64_t).
};

/**
 * @brief Represents a single column definition in a DNT table schema.
 */
struct DNTColumn {
    std::string name;                          ///< Column identifier name (e.g., "_ItemID", "_LevelLimit").
    DNTFieldType type = DNTFieldType::INT;     ///< Field data type.
    uint32_t slotIndex = 0;                    ///< Index within the row's numeric or string slice in contiguous arenas.
};

/**
 * @brief Lightweight 8-byte handle referencing a row stored inside DNTTable's slab arenas.
 */
struct DNTRow {
    int32_t rowId = 0;     ///< Primary key (Row ID, e.g. Item ID).
    uint32_t slabRow = 0;  ///< Offset index into the contiguous memory arena.
};
static_assert(sizeof(DNTRow) == 8, "DNTRow must remain exactly 8 bytes for cache efficiency");

/**
 * @class DNTTable
 * @brief In-memory representation of a Dragon Nest .dnt binary table.
 *
 * Features slab-allocated column storage with split numeric and string arenas,
 * O(1) row lookups by ID, and byte-exact binary serialization preserving file headers,
 * column order, and optional reserve blocks.
 */
class DNTTable {
public:
    DNTTable() = default;

    /**
     * @brief Parses and loads a .dnt binary table from disk into memory arenas.
     * @param path Filesystem path to the .dnt file.
     * @return true if successfully parsed, false otherwise.
     */
    bool LoadFromFile(const std::filesystem::path& path);

    /**
     * @brief Serializes the table to disk in the binary DNT format.
     * @param path Target filesystem destination path.
     * @param quiet If true, suppresses non-error diagnostic log output.
     * @return true if written successfully, false on I/O failure.
     */
    bool SaveToFile(const std::filesystem::path& path, bool quiet = false) const;

    /** @brief Returns true if this table is an uninitialized empty placeholder. */
    bool IsEmptyPlaceholder() const noexcept { return m_emptyPlaceholder; }

    /** @brief Returns true if the table has a valid schema with at least one column. */
    bool HasSchema() const noexcept { return !m_columns.empty(); }

    /** @brief Checks if a row with the given ID exists in the table. */
    bool HasRow(int32_t rowId) const;

    /** @brief Retrieves a pointer to the row handle by its ID, or nullptr if not found. */
    DNTRow* GetRow(int32_t rowId);

    /** @brief Retrieves a const pointer to the row handle by its ID, or nullptr if not found. */
    const DNTRow* GetRow(int32_t rowId) const;

    /**
     * @brief Resolves a column name to its 0-based column index.
     * @param colName Case-sensitive column name.
     * @return 0-based index if found, -1 if the column does not exist.
     */
    int32_t GetColumnIndex(std::string_view colName) const;

    /** @brief Returns the ordered vector of column descriptors in the schema. */
    const std::vector<DNTColumn>& GetColumns() const noexcept { return m_columns; }

    /** @brief Retrieves the data type of a column by its index. */
    DNTFieldType GetColumnType(size_t colIdx) const noexcept;

    /** @brief Returns a human-readable display string for a DNTFieldType. */
    static const char* ColumnTypeName(DNTFieldType type) noexcept;

    /** @brief Returns all row handles in the table in insertion order. */
    const std::vector<DNTRow>& GetRows() const noexcept { return m_rows; }

    /** @brief Returns the total number of rows in the table. */
    size_t GetRowCount() const noexcept { return m_rows.size(); }

    // --- Typed cell accessors ---

    /** @brief Reads a 32-bit integer cell value. */
    int32_t GetInt(const DNTRow& row, size_t colIdx, int32_t def = 0) const noexcept;

    /** @brief Reads a 64-bit integer cell value. */
    int64_t GetInt64(const DNTRow& row, size_t colIdx, int64_t def = 0) const noexcept;

    /** @brief Reads a single-precision floating point cell value. */
    float GetFloat(const DNTRow& row, size_t colIdx, float def = 0.0f) const noexcept;

    /** @brief Reads a string cell value. */
    const std::string& GetString(const DNTRow& row, size_t colIdx) const noexcept;

    // --- Typed cell mutators ---

    void SetInt(DNTRow& row, size_t colIdx, int32_t val) noexcept;
    void SetInt64(DNTRow& row, size_t colIdx, int64_t val) noexcept;
    void SetFloat(DNTRow& row, size_t colIdx, float val) noexcept;
    void SetString(DNTRow& row, size_t colIdx, std::string_view val);

    void SetInt(int32_t rowId, int colIdx, int32_t val) noexcept;
    void SetInt64(int32_t rowId, int colIdx, int64_t val) noexcept;
    void SetFloat(int32_t rowId, int colIdx, float val) noexcept;
    void SetString(int32_t rowId, int colIdx, std::string_view val);

    // --- Cell string conversion ---

    /** @brief Formats a cell value into its display string representation. */
    std::string FormatCell(const DNTRow& row, size_t colIdx) const;

    /** @brief Parses a string and assigns it to the designated cell based on column type. */
    bool ParseAndSetCell(int32_t rowId, size_t colIdx, std::string_view textValue);

    // --- Row lifecycle operations ---

    /** @brief Creates and appends a new row initialized with zero/empty values. */
    DNTRow* CreateDefaultRow(int32_t newRowId);

    /** @brief Clones an existing row within this table into a new destination row ID. */
    bool CloneRow(int32_t srcId, int32_t dstId);

    /** @brief Clones a row from a compatible source table into this table. */
    bool CloneRowFrom(const DNTTable& srcTable, int32_t srcId, int32_t dstId);

    /** @brief Deletes a row by its ID. */
    bool DeleteRow(int32_t rowId);

    /** @brief Returns the source file path from which the table was loaded. */
    const std::filesystem::path& GetPath() const noexcept { return m_filePath; }

    /** @brief Returns the filename of the table (e.g., "itemtable_equipment.dnt"). */
    const std::string& GetFileName() const noexcept { return m_fileName; }

private:
    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
    };

    uint32_t AllocateSlabRow();

    const uint64_t* RowNumerics(uint32_t slabRow) const noexcept {
        return m_numericArena.data() + static_cast<size_t>(slabRow) * m_numericColCount;
    }
    uint64_t* RowNumerics(uint32_t slabRow) noexcept {
        return m_numericArena.data() + static_cast<size_t>(slabRow) * m_numericColCount;
    }
    const std::string* RowStrings(uint32_t slabRow) const noexcept {
        return m_stringArena.data() + static_cast<size_t>(slabRow) * m_stringColCount;
    }
    std::string* RowStrings(uint32_t slabRow) noexcept {
        return m_stringArena.data() + static_cast<size_t>(slabRow) * m_stringColCount;
    }

    std::filesystem::path m_filePath;
    std::string m_fileName;
    bool m_emptyPlaceholder = false;
    int16_t m_version = 1;
    uint32_t m_numericColCount = 0;
    uint32_t m_stringColCount = 0;
    uint32_t m_fixedRowBytes = 4; ///< Serialized byte count per row excluding variable string bodies.
    uint32_t m_slabRowsAllocated = 0;

    std::vector<char> m_reserveBytes;
    std::vector<char> m_footerBytes;

    std::vector<DNTColumn> m_columns;
    std::unordered_map<std::string, size_t, StringHash, std::equal_to<>> m_columnMap;

    std::vector<DNTRow> m_rows;
    std::vector<uint64_t> m_numericArena;
    std::vector<std::string> m_stringArena;
    std::unordered_map<int32_t, uint32_t> m_rowIndexById;
};

} // namespace DNItemMaker::Data
