#include "DNTReader.h"
#include "Common/Logs/Logs.h"

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>

namespace DNItemMaker::Data {

namespace {
const std::string kEmptyString;
constexpr char kDefaultFooter[6] = { 0x05, 'T', 'H', 'E', 'N', 'D' };

std::string PathFilenameUtf8(const std::filesystem::path& p) {
    const auto u8 = p.filename().u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}
} // namespace

const char* DNTTable::ColumnTypeName(DNTFieldType type) noexcept {
    switch (type) {
        case DNTFieldType::NA:     return "NA";
        case DNTFieldType::STRING: return "STRING";
        case DNTFieldType::BOOL:   return "BOOL";
        case DNTFieldType::INT:    return "INT";
        case DNTFieldType::PER:    return "PER (float)";
        case DNTFieldType::FLOAT:  return "FLOAT";
        case DNTFieldType::INT64:  return "INT64";
        default:                   return "UNKNOWN";
    }
}

uint32_t DNTTable::AllocateSlabRow() {
    const uint32_t slab = m_slabRowsAllocated++;
    if (m_numericColCount > 0) {
        m_numericArena.resize(static_cast<size_t>(m_slabRowsAllocated) * m_numericColCount, 0ull);
    }
    if (m_stringColCount > 0) {
        m_stringArena.resize(static_cast<size_t>(m_slabRowsAllocated) * m_stringColCount);
    }
    return slab;
}

bool DNTTable::LoadFromFile(const std::filesystem::path& path) {
    m_filePath = path;
    m_fileName = PathFilenameUtf8(path);
    m_emptyPlaceholder = false;
    m_version = 1;
    m_numericColCount = 0;
    m_stringColCount = 0;
    m_fixedRowBytes = 4;
    m_slabRowsAllocated = 0;
    m_reserveBytes.clear();
    m_footerBytes.clear();
    m_columns.clear();
    m_columnMap.clear();
    m_rows.clear();
    m_numericArena.clear();
    m_stringArena.clear();
    m_rowIndexById.clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR(Channels::dnt, "Failed to open DNT table: {}", path.string());
        return false;
    }

    const std::streamoff fileSizeSigned = file.tellg();
    if (fileSizeSigned < 0) {
        LOG_ERROR(Channels::dnt, "Failed to stat DNT table: {}", path.string());
        return false;
    }
    const auto fileSize = static_cast<size_t>(fileSizeSigned);

    // Gracefully handle 0-byte shipped placeholder tables (e.g. weapontable_reboot.dnt, partstable_amp.dnt).
    if (fileSize == 0) {
        m_emptyPlaceholder = true;
        return true;
    }

    if (fileSize < 10) {
        LOG_ERROR(Channels::dnt, "Truncated DNT file ({} bytes): {}", fileSize, path.string());
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(fileSize);
    if (!file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(fileSize))) {
        LOG_ERROR(Channels::dnt, "Failed to read DNT file buffer: {}", path.string());
        return false;
    }

    const uint8_t* ptr = buf.data();
    const uint8_t* const end = ptr + fileSize;
    auto canRead = [&](size_t n) noexcept -> bool { return static_cast<size_t>(end - ptr) >= n; };

    int16_t version = 0;
    int16_t reserveLen = 0;
    std::memcpy(&version, ptr, 2); ptr += 2;
    std::memcpy(&reserveLen, ptr, 2); ptr += 2;
    m_version = version;

    if (reserveLen > 0) {
        if (!canRead(static_cast<size_t>(reserveLen))) return false;
        m_reserveBytes.assign(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr + reserveLen));
        ptr += reserveLen;
    }

    if (!canRead(6)) return false;
    int16_t fieldCount = 0;
    int32_t dataCount = 0;
    std::memcpy(&fieldCount, ptr, 2); ptr += 2;
    std::memcpy(&dataCount, ptr, 4); ptr += 4;

    if (fieldCount <= 0 || dataCount < 0) {
        LOG_ERROR(Channels::dnt, "Invalid DNT header in {}: fieldCount={}, dataCount={}",
                  m_fileName, fieldCount, dataCount);
        return false;
    }

    m_columns.resize(static_cast<size_t>(fieldCount));
    m_columnMap.reserve(static_cast<size_t>(fieldCount) * 2);

    uint32_t fixedRowBytes = 4; // 4-byte rowId prefix
    for (int16_t i = 0; i < fieldCount; ++i) {
        if (!canRead(2)) return false;
        int16_t nameLen = 0;
        std::memcpy(&nameLen, ptr, 2); ptr += 2;
        if (nameLen < 0 || !canRead(static_cast<size_t>(nameLen) + 1)) return false;

        std::string colName(reinterpret_cast<const char*>(ptr), static_cast<size_t>(nameLen));
        ptr += nameLen;

        const auto fieldType = static_cast<DNTFieldType>(*ptr++);
        uint32_t slot = 0;
        if (fieldType == DNTFieldType::STRING) {
            slot = m_stringColCount++;
            fixedRowBytes += 2; // int16_t length prefix
        } else {
            slot = m_numericColCount++;
            fixedRowBytes += (fieldType == DNTFieldType::INT64) ? 8 : 4;
        }

        m_columns[static_cast<size_t>(i)] = { std::move(colName), fieldType, slot };
        m_columnMap.emplace(m_columns[static_cast<size_t>(i)].name, static_cast<size_t>(i));
    }
    m_fixedRowBytes = fixedRowBytes;

    const auto rowCount = static_cast<size_t>(dataCount);
    m_rows.resize(rowCount);
    m_slabRowsAllocated = static_cast<uint32_t>(rowCount);
    if (m_numericColCount > 0) {
        m_numericArena.assign(rowCount * m_numericColCount, 0ull);
    }
    if (m_stringColCount > 0) {
        m_stringArena.resize(rowCount * m_stringColCount);
    }
    m_rowIndexById.reserve(rowCount + 16);

    const DNTColumn* const cols = m_columns.data();
    for (int32_t r = 0; r < dataCount; ++r) {
        if (!canRead(4)) {
            m_rows.resize(static_cast<size_t>(r));
            break;
        }

        const auto rIdx = static_cast<uint32_t>(r);
        DNTRow& row = m_rows[rIdx];
        std::memcpy(&row.rowId, ptr, 4); ptr += 4;
        row.slabRow = rIdx;

        uint64_t* const rowNums = m_numericColCount > 0 ? RowNumerics(rIdx) : nullptr;
        std::string* const rowStrs = m_stringColCount > 0 ? RowStrings(rIdx) : nullptr;

        for (int16_t c = 0; c < fieldCount; ++c) {
            const DNTColumn& col = cols[c];
            switch (col.type) {
                case DNTFieldType::STRING: {
                    if (!canRead(2)) return false;
                    int16_t strLen = 0;
                    std::memcpy(&strLen, ptr, 2); ptr += 2;
                    if (strLen > 0) {
                        if (!canRead(static_cast<size_t>(strLen))) return false;
                        rowStrs[col.slotIndex].assign(reinterpret_cast<const char*>(ptr), static_cast<size_t>(strLen));
                        ptr += strLen;
                    }
                    break;
                }
                case DNTFieldType::PER:
                case DNTFieldType::FLOAT: {
                    if (!canRead(4)) return false;
                    uint32_t bits = 0;
                    std::memcpy(&bits, ptr, 4); ptr += 4;
                    rowNums[col.slotIndex] = bits;
                    break;
                }
                case DNTFieldType::INT64: {
                    if (!canRead(8)) return false;
                    uint64_t bits = 0;
                    std::memcpy(&bits, ptr, 8); ptr += 8;
                    rowNums[col.slotIndex] = bits;
                    break;
                }
                case DNTFieldType::NA:
                case DNTFieldType::BOOL:
                case DNTFieldType::INT:
                default: {
                    if (!canRead(4)) return false;
                    int32_t iVal = 0;
                    std::memcpy(&iVal, ptr, 4); ptr += 4;
                    rowNums[col.slotIndex] = static_cast<uint64_t>(static_cast<int64_t>(iVal));
                    break;
                }
            }
        }

        m_rowIndexById[row.rowId] = rIdx;
    }

    if (ptr < end) {
        m_footerBytes.assign(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(end));
    } else {
        m_footerBytes.assign(kDefaultFooter, kDefaultFooter + sizeof(kDefaultFooter));
    }

    return true;
}

bool DNTTable::SaveToFile(const std::filesystem::path& path, bool quiet) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        LOG_ERROR(Channels::dnt, "Failed to open DNT for writing: {}", path.string());
        return false;
    }

    if (m_emptyPlaceholder && m_columns.empty()) {
        file.flush();
        return file.good();
    }

    // Precalculate total output buffer size.
    size_t exactBytes = 10 + m_reserveBytes.size();
    for (const auto& col : m_columns) {
        exactBytes += 3 + col.name.size();
    }
    exactBytes += m_rows.size() * m_fixedRowBytes;
    if (m_stringColCount > 0) {
        for (const auto& row : m_rows) {
            const std::string* const strs = RowStrings(row.slabRow);
            for (uint32_t s = 0; s < m_stringColCount; ++s) {
                exactBytes += strs[s].size();
            }
        }
    }
    const size_t footerLen = m_footerBytes.empty() ? sizeof(kDefaultFooter) : m_footerBytes.size();
    exactBytes += footerLen;

    std::vector<char> out(exactBytes);
    char* dst = out.data();

    auto writeRaw = [&](const void* src, size_t len) noexcept {
        std::memcpy(dst, src, len);
        dst += len;
    };

    const int16_t version = m_version;
    const auto reserveLen = static_cast<int16_t>(m_reserveBytes.size());
    const auto fieldCount = static_cast<int16_t>(m_columns.size());
    const auto dataCount = static_cast<int32_t>(m_rows.size());

    writeRaw(&version, 2);
    writeRaw(&reserveLen, 2);
    if (reserveLen > 0) writeRaw(m_reserveBytes.data(), static_cast<size_t>(reserveLen));
    writeRaw(&fieldCount, 2);
    writeRaw(&dataCount, 4);

    for (const auto& col : m_columns) {
        const auto nameLen = static_cast<int16_t>(col.name.size());
        const auto typeByte = static_cast<uint8_t>(col.type);
        writeRaw(&nameLen, 2);
        if (nameLen > 0) writeRaw(col.name.data(), static_cast<size_t>(nameLen));
        writeRaw(&typeByte, 1);
    }

    const DNTColumn* const cols = m_columns.data();
    const size_t colCount = m_columns.size();

    for (const auto& row : m_rows) {
        writeRaw(&row.rowId, 4);
        const uint64_t* const rowNums = m_numericColCount > 0 ? RowNumerics(row.slabRow) : nullptr;
        const std::string* const rowStrs = m_stringColCount > 0 ? RowStrings(row.slabRow) : nullptr;

        for (size_t c = 0; c < colCount; ++c) {
            const DNTColumn& col = cols[c];
            switch (col.type) {
                case DNTFieldType::STRING: {
                    const std::string& s = rowStrs[col.slotIndex];
                    const auto sLen = static_cast<int16_t>(s.size());
                    writeRaw(&sLen, 2);
                    if (sLen > 0) writeRaw(s.data(), static_cast<size_t>(sLen));
                    break;
                }
                case DNTFieldType::PER:
                case DNTFieldType::FLOAT: {
                    const auto bits = static_cast<uint32_t>(rowNums[col.slotIndex]);
                    writeRaw(&bits, 4);
                    break;
                }
                case DNTFieldType::INT64: {
                    const uint64_t bits = rowNums[col.slotIndex];
                    writeRaw(&bits, 8);
                    break;
                }
                case DNTFieldType::NA:
                case DNTFieldType::BOOL:
                case DNTFieldType::INT:
                default: {
                    const auto iVal = static_cast<int32_t>(static_cast<int64_t>(rowNums[col.slotIndex]));
                    writeRaw(&iVal, 4);
                    break;
                }
            }
        }
    }

    if (!m_footerBytes.empty()) {
        writeRaw(m_footerBytes.data(), m_footerBytes.size());
    } else {
        writeRaw(kDefaultFooter, sizeof(kDefaultFooter));
    }

    file.write(out.data(), static_cast<std::streamsize>(out.size()));
    file.flush();
    if (!file.good()) {
        LOG_ERROR(Channels::dnt, "I/O error while writing DNT '{}'", path.string());
        return false;
    }

    if (!quiet) {
        LOG_INFO(Channels::dnt, "Saved DNT '{}' ({} rows, {} bytes)",
                 PathFilenameUtf8(path), dataCount, out.size());
    }
    return true;
}

bool DNTTable::HasRow(int32_t rowId) const {
    return m_rowIndexById.contains(rowId);
}

DNTRow* DNTTable::GetRow(int32_t rowId) {
    const auto it = m_rowIndexById.find(rowId);
    return it != m_rowIndexById.end() ? &m_rows[it->second] : nullptr;
}

const DNTRow* DNTTable::GetRow(int32_t rowId) const {
    const auto it = m_rowIndexById.find(rowId);
    return it != m_rowIndexById.end() ? &m_rows[it->second] : nullptr;
}

int32_t DNTTable::GetColumnIndex(std::string_view colName) const {
    const auto it = m_columnMap.find(colName);
    return it != m_columnMap.end() ? static_cast<int32_t>(it->second) : -1;
}

DNTFieldType DNTTable::GetColumnType(size_t colIdx) const noexcept {
    return colIdx < m_columns.size() ? m_columns[colIdx].type : DNTFieldType::NA;
}

int32_t DNTTable::GetInt(const DNTRow& row, size_t colIdx, int32_t def) const noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return def;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return def;
    const uint64_t raw = RowNumerics(row.slabRow)[col.slotIndex];
    if (col.type == DNTFieldType::FLOAT || col.type == DNTFieldType::PER) {
        const auto bits = static_cast<uint32_t>(raw);
        float f = 0.0f;
        std::memcpy(&f, &bits, 4);
        return static_cast<int32_t>(f);
    }
    return static_cast<int32_t>(static_cast<int64_t>(raw));
}

int64_t DNTTable::GetInt64(const DNTRow& row, size_t colIdx, int64_t def) const noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return def;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return def;
    return static_cast<int64_t>(RowNumerics(row.slabRow)[col.slotIndex]);
}

float DNTTable::GetFloat(const DNTRow& row, size_t colIdx, float def) const noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return def;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return def;
    const uint64_t raw = RowNumerics(row.slabRow)[col.slotIndex];
    if (col.type == DNTFieldType::FLOAT || col.type == DNTFieldType::PER) {
        const auto bits = static_cast<uint32_t>(raw);
        float f = 0.0f;
        std::memcpy(&f, &bits, 4);
        return f;
    }
    return static_cast<float>(static_cast<int64_t>(raw));
}

const std::string& DNTTable::GetString(const DNTRow& row, size_t colIdx) const noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return kEmptyString;
    const auto& col = m_columns[colIdx];
    if (col.type != DNTFieldType::STRING || col.slotIndex >= m_stringColCount) return kEmptyString;
    return RowStrings(row.slabRow)[col.slotIndex];
}

void DNTTable::SetInt(DNTRow& row, size_t colIdx, int32_t val) noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return;
    uint64_t& dst = RowNumerics(row.slabRow)[col.slotIndex];
    if (col.type == DNTFieldType::FLOAT || col.type == DNTFieldType::PER) {
        const auto f = static_cast<float>(val);
        uint32_t bits = 0;
        std::memcpy(&bits, &f, 4);
        dst = bits;
    } else {
        dst = static_cast<uint64_t>(static_cast<int64_t>(val));
    }
}

void DNTTable::SetInt64(DNTRow& row, size_t colIdx, int64_t val) noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return;
    RowNumerics(row.slabRow)[col.slotIndex] = static_cast<uint64_t>(val);
}

void DNTTable::SetFloat(DNTRow& row, size_t colIdx, float val) noexcept {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return;
    const auto& col = m_columns[colIdx];
    if (col.type == DNTFieldType::STRING || col.slotIndex >= m_numericColCount) return;
    uint64_t& dst = RowNumerics(row.slabRow)[col.slotIndex];
    if (col.type == DNTFieldType::FLOAT || col.type == DNTFieldType::PER) {
        uint32_t bits = 0;
        std::memcpy(&bits, &val, 4);
        dst = bits;
    } else {
        dst = static_cast<uint64_t>(static_cast<int64_t>(val));
    }
}

void DNTTable::SetString(DNTRow& row, size_t colIdx, std::string_view val) {
    if (colIdx >= m_columns.size() || row.slabRow >= m_slabRowsAllocated) return;
    const auto& col = m_columns[colIdx];
    if (col.type != DNTFieldType::STRING || col.slotIndex >= m_stringColCount) return;
    RowStrings(row.slabRow)[col.slotIndex].assign(val.data(), val.size());
}

void DNTTable::SetInt(int32_t rowId, int colIdx, int32_t val) noexcept {
    if (colIdx < 0) return;
    if (auto* r = GetRow(rowId)) SetInt(*r, static_cast<size_t>(colIdx), val);
}

void DNTTable::SetInt64(int32_t rowId, int colIdx, int64_t val) noexcept {
    if (colIdx < 0) return;
    if (auto* r = GetRow(rowId)) SetInt64(*r, static_cast<size_t>(colIdx), val);
}

void DNTTable::SetFloat(int32_t rowId, int colIdx, float val) noexcept {
    if (colIdx < 0) return;
    if (auto* r = GetRow(rowId)) SetFloat(*r, static_cast<size_t>(colIdx), val);
}

void DNTTable::SetString(int32_t rowId, int colIdx, std::string_view val) {
    if (colIdx < 0) return;
    if (auto* r = GetRow(rowId)) SetString(*r, static_cast<size_t>(colIdx), val);
}

std::string DNTTable::FormatCell(const DNTRow& row, size_t colIdx) const {
    if (colIdx >= m_columns.size()) return {};
    switch (m_columns[colIdx].type) {
        case DNTFieldType::STRING:
            return GetString(row, colIdx);
        case DNTFieldType::PER:
        case DNTFieldType::FLOAT:
            return std::format("{:.6g}", GetFloat(row, colIdx));
        case DNTFieldType::INT64:
            return std::to_string(GetInt64(row, colIdx));
        case DNTFieldType::BOOL:
        case DNTFieldType::INT:
        case DNTFieldType::NA:
        default:
            return std::to_string(GetInt(row, colIdx));
    }
}

bool DNTTable::ParseAndSetCell(int32_t rowId, size_t colIdx, std::string_view textValue) {
    auto* row = GetRow(rowId);
    if (!row || colIdx >= m_columns.size()) return false;

    const auto type = m_columns[colIdx].type;
    if (type == DNTFieldType::STRING) {
        SetString(*row, colIdx, textValue);
        return true;
    }

    while (!textValue.empty() && (textValue.front() == ' ' || textValue.front() == '\t')) textValue.remove_prefix(1);
    while (!textValue.empty() && (textValue.back() == ' ' || textValue.back() == '\t')) textValue.remove_suffix(1);

    if (type == DNTFieldType::PER || type == DNTFieldType::FLOAT) {
        if (textValue.empty()) {
            SetFloat(*row, colIdx, 0.0f);
            return true;
        }
        float f = 0.0f;
        const auto [p, ec] = std::from_chars(textValue.data(), textValue.data() + textValue.size(), f);
        if (ec != std::errc{} || p != textValue.data() + textValue.size()) return false;
        SetFloat(*row, colIdx, f);
        return true;
    }

    if (type == DNTFieldType::INT64) {
        if (textValue.empty()) {
            SetInt64(*row, colIdx, 0);
            return true;
        }
        int64_t v = 0;
        const auto [p, ec] = std::from_chars(textValue.data(), textValue.data() + textValue.size(), v);
        if (ec != std::errc{} || p != textValue.data() + textValue.size()) return false;
        SetInt64(*row, colIdx, v);
        return true;
    }

    if (type == DNTFieldType::BOOL) {
        if (textValue == "true" || textValue == "TRUE" || textValue == "True") {
            SetInt(*row, colIdx, 1);
            return true;
        }
        if (textValue == "false" || textValue == "FALSE" || textValue == "False") {
            SetInt(*row, colIdx, 0);
            return true;
        }
    }

    if (textValue.empty()) {
        SetInt(*row, colIdx, 0);
        return true;
    }
    int32_t v = 0;
    const auto [p, ec] = std::from_chars(textValue.data(), textValue.data() + textValue.size(), v);
    if (ec != std::errc{} || p != textValue.data() + textValue.size()) return false;
    SetInt(*row, colIdx, v);
    return true;
}

DNTRow* DNTTable::CreateDefaultRow(int32_t newRowId) {
    if (m_columns.empty() || HasRow(newRowId)) return nullptr;

    DNTRow row;
    row.rowId = newRowId;
    row.slabRow = AllocateSlabRow();

    for (size_t i = 0; i < m_columns.size(); ++i) {
        const auto& name = m_columns[i].name;
        if (name.starts_with("_State") && name.find('_', 6) == std::string::npos) {
            SetInt(row, i, -1);
        } else if (name == "_LevelLimit" || name == "_OverlapCount" || name == "_AbleStorage" || name == "_AbleWStorage") {
            SetInt(row, i, 1);
        } else if (name == "_NeedJobClass") {
            SetString(row, i, "0");
        }
    }

    const auto idx = static_cast<uint32_t>(m_rows.size());
    m_rows.push_back(row);
    m_rowIndexById[newRowId] = idx;
    m_emptyPlaceholder = false;
    return &m_rows.back();
}

bool DNTTable::CloneRow(int32_t srcId, int32_t dstId) {
    const auto* src = GetRow(srcId);
    if (!src || HasRow(dstId)) return false;

    const uint32_t srcSlab = src->slabRow;
    const uint32_t dstSlab = AllocateSlabRow();

    if (m_numericColCount > 0) {
        std::memcpy(RowNumerics(dstSlab), RowNumerics(srcSlab), static_cast<size_t>(m_numericColCount) * sizeof(uint64_t));
    }
    if (m_stringColCount > 0) {
        const std::string* sPtr = RowStrings(srcSlab);
        std::string* dPtr = RowStrings(dstSlab);
        for (uint32_t i = 0; i < m_stringColCount; ++i) {
            dPtr[i] = sPtr[i];
        }
    }

    const auto idx = static_cast<uint32_t>(m_rows.size());
    m_rows.push_back(DNTRow{ dstId, dstSlab });
    m_rowIndexById[dstId] = idx;
    return true;
}

bool DNTTable::CloneRowFrom(const DNTTable& srcTable, int32_t srcId, int32_t dstId) {
    if (&srcTable == this) return CloneRow(srcId, dstId);

    const auto* src = srcTable.GetRow(srcId);
    if (!src || m_columns.empty() || HasRow(dstId)) return false;

    bool identicalSchema = (srcTable.m_columns.size() == m_columns.size() &&
                            srcTable.m_numericColCount == m_numericColCount &&
                            srcTable.m_stringColCount == m_stringColCount);
    for (size_t i = 0; identicalSchema && i < m_columns.size(); ++i) {
        if (srcTable.m_columns[i].type != m_columns[i].type ||
            srcTable.m_columns[i].slotIndex != m_columns[i].slotIndex ||
            srcTable.m_columns[i].name != m_columns[i].name) {
            identicalSchema = false;
        }
    }

    if (identicalSchema) {
        const uint32_t srcSlab = src->slabRow;
        const uint32_t dstSlab = AllocateSlabRow();
        if (m_numericColCount > 0) {
            std::memcpy(RowNumerics(dstSlab), srcTable.RowNumerics(srcSlab),
                        static_cast<size_t>(m_numericColCount) * sizeof(uint64_t));
        }
        if (m_stringColCount > 0) {
            const std::string* sPtr = srcTable.RowStrings(srcSlab);
            std::string* dPtr = RowStrings(dstSlab);
            for (uint32_t i = 0; i < m_stringColCount; ++i) {
                dPtr[i] = sPtr[i];
            }
        }
        const auto idx = static_cast<uint32_t>(m_rows.size());
        m_rows.push_back(DNTRow{ dstId, dstSlab });
        m_rowIndexById[dstId] = idx;
        return true;
    }

    DNTRow* dst = CreateDefaultRow(dstId);
    if (!dst) return false;
    for (size_t dstCol = 0; dstCol < m_columns.size(); ++dstCol) {
        const int32_t srcCol = srcTable.GetColumnIndex(m_columns[dstCol].name);
        if (srcCol < 0) continue;
        const auto sIdx = static_cast<size_t>(srcCol);
        switch (m_columns[dstCol].type) {
            case DNTFieldType::STRING:
                SetString(*dst, dstCol, srcTable.GetString(*src, sIdx));
                break;
            case DNTFieldType::PER:
            case DNTFieldType::FLOAT:
                SetFloat(*dst, dstCol, srcTable.GetFloat(*src, sIdx));
                break;
            case DNTFieldType::INT64:
                SetInt64(*dst, dstCol, srcTable.GetInt64(*src, sIdx));
                break;
            default:
                SetInt(*dst, dstCol, srcTable.GetInt(*src, sIdx));
                break;
        }
    }
    return true;
}

bool DNTTable::DeleteRow(int32_t rowId) {
    const auto it = m_rowIndexById.find(rowId);
    if (it == m_rowIndexById.end()) return false;

    const uint32_t idx = it->second;
    m_rowIndexById.erase(it);
    m_rows.erase(m_rows.begin() + idx);
    for (uint32_t i = idx; i < static_cast<uint32_t>(m_rows.size()); ++i) {
        m_rowIndexById[m_rows[i].rowId] = i;
    }
    return true;
}

} // namespace DNItemMaker::Data
