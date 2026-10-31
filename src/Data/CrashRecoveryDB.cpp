#include "CrashRecoveryDB.h"

#include "Common/Logs/Logs.h"

#include <SQLiteCpp/SQLiteCpp.h>

#include <charconv>
#include <format>

namespace DNItemMaker::Data {

namespace {

std::string EncodeStates(const DNItem& item) {
    std::string out;
    out.reserve(160);
    for (const auto& s : item.states) {
        if (!out.empty()) out += ';';
        out += std::format("{}|{}|{}|{}", s.type, s.genProb, s.minValue, s.maxValue);
    }
    return out;
}

std::string_view NextToken(std::string_view& rest, char delim) noexcept {
    const size_t pos = rest.find(delim);
    if (pos == std::string_view::npos) {
        const std::string_view tok = rest;
        rest = {};
        return tok;
    }
    const std::string_view tok = rest.substr(0, pos);
    rest.remove_prefix(pos + 1);
    return tok;
}

void DecodeStates(std::string_view text, DNItem& item) {
    size_t i = 0;
    while (i < kStateSlotCount && !text.empty()) {
        std::string_view row = NextToken(text, ';');
        const std::string_view typeSv = NextToken(row, '|');
        const std::string_view probSv = NextToken(row, '|');
        const std::string_view minSv = NextToken(row, '|');
        const std::string_view maxSv = NextToken(row, '|');

        int32_t parsedType = -1;
        float parsedProb = 0.0f;
        const auto [tpPtr, tpEc] = std::from_chars(typeSv.data(), typeSv.data() + typeSv.size(), parsedType);
        const auto [prPtr, prEc] = std::from_chars(probSv.data(), probSv.data() + probSv.size(), parsedProb);
        if (tpEc != std::errc{} || prEc != std::errc{}) {
            ++i;
            continue;
        }

        item.states[i].type = parsedType;
        item.states[i].genProb = parsedProb;
        item.states[i].minValue.assign(minSv);
        item.states[i].maxValue.assign(maxSv);
        ++i;
    }
}

} // namespace

CrashRecoveryDB::CrashRecoveryDB() = default;
CrashRecoveryDB::~CrashRecoveryDB() = default;

bool CrashRecoveryDB::Initialize(const std::filesystem::path& dbPath) {
    try {
        m_db = std::make_unique<SQLite::Database>(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        m_db->exec("PRAGMA journal_mode = WAL;");
        m_db->exec("PRAGMA synchronous = NORMAL;");
        m_db->exec("CREATE TABLE IF NOT EXISTS drafts ("
                   " item_id INTEGER PRIMARY KEY,"
                   " name TEXT, name_template TEXT, name_param TEXT,"
                   " description TEXT, desc_template TEXT, desc_param TEXT,"
                   " rank INTEGER, level INTEGER, hero_level INTEGER, job TEXT,"
                   " overlap INTEGER, sell INTEGER, durability INTEGER, gear_score INTEGER,"
                   " flags INTEGER, states TEXT, icon_index INTEGER DEFAULT -1,"
                   " saved_at DATETIME DEFAULT CURRENT_TIMESTAMP);");

        // Backward-compatible schema upgrade if an older database lacked icon_index.
        try {
            m_db->exec("ALTER TABLE drafts ADD COLUMN icon_index INTEGER DEFAULT -1;");
        } catch (...) {
            // Column already exists.
        }

        m_db->exec("CREATE TABLE IF NOT EXISTS settings ("
                   " key TEXT PRIMARY KEY,"
                   " value TEXT NOT NULL);");

        m_initialized = true;
        RefreshDraftCount();
        LOG_INFO(Channels::sqlite, "Recovery journal ready: {} ({} draft(s))", dbPath.string(), m_draftCount);
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR(Channels::sqlite, "Cannot open recovery journal: {}", ex.what());
        m_initialized = false;
        m_draftCount = 0;
        return false;
    }
}

bool CrashRecoveryDB::SaveDraft(const DNItem& item) {
    if (!m_initialized) return false;
    try {
        SQLite::Statement q(*m_db,
                            "INSERT OR REPLACE INTO drafts (item_id, name, name_template, name_param, description, desc_template, desc_param,"
                            " rank, level, hero_level, job, overlap, sell, durability, gear_score, flags, states, icon_index, saved_at)"
                            " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,CURRENT_TIMESTAMP);");
        q.bind(1, item.id);
        q.bind(2, item.name);
        q.bind(3, item.nameTemplate);
        q.bind(4, item.nameParam);
        q.bind(5, item.description);
        q.bind(6, item.descTemplate);
        q.bind(7, item.descParam);
        q.bind(8, item.rank);
        q.bind(9, item.levelLimit);
        q.bind(10, item.heroLevelLimit);
        q.bind(11, item.jobClass);
        q.bind(12, item.overlapCount);
        q.bind(13, item.sellAmount);
        q.bind(14, item.durability);
        q.bind(15, item.gearScore);
        q.bind(16, (item.isCash ? 1 : 0) | (item.ableStorage ? 2 : 0) | (item.ableWStorage ? 4 : 0));
        q.bind(17, EncodeStates(item));
        q.bind(18, item.iconIndex);
        q.exec();
        RefreshDraftCount();
        return true;
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Draft save failed: {}", ex.what());
        return false;
    }
}

bool CrashRecoveryDB::SaveDraftsBatch(const std::vector<const DNItem*>& items) {
    if (!m_initialized || items.empty()) return false;
    try {
        SQLite::Transaction tx(*m_db);
        SQLite::Statement q(*m_db,
                            "INSERT OR REPLACE INTO drafts (item_id, name, name_template, name_param, description, desc_template, desc_param,"
                            " rank, level, hero_level, job, overlap, sell, durability, gear_score, flags, states, icon_index, saved_at)"
                            " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,CURRENT_TIMESTAMP);");
        for (const DNItem* ptr : items) {
            if (!ptr) continue;
            const DNItem& item = *ptr;
            q.reset();
            q.clearBindings();
            q.bind(1, item.id);
            q.bind(2, item.name);
            q.bind(3, item.nameTemplate);
            q.bind(4, item.nameParam);
            q.bind(5, item.description);
            q.bind(6, item.descTemplate);
            q.bind(7, item.descParam);
            q.bind(8, item.rank);
            q.bind(9, item.levelLimit);
            q.bind(10, item.heroLevelLimit);
            q.bind(11, item.jobClass);
            q.bind(12, item.overlapCount);
            q.bind(13, item.sellAmount);
            q.bind(14, item.durability);
            q.bind(15, item.gearScore);
            q.bind(16, (item.isCash ? 1 : 0) | (item.ableStorage ? 2 : 0) | (item.ableWStorage ? 4 : 0));
            q.bind(17, EncodeStates(item));
            q.bind(18, item.iconIndex);
            q.exec();
        }
        tx.commit();
        RefreshDraftCount();
        return true;
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Batch draft save failed: {}", ex.what());
        return false;
    }
}

void CrashRecoveryDB::RemoveDraft(int32_t itemId) {
    if (!m_initialized) return;
    try {
        SQLite::Statement q(*m_db, "DELETE FROM drafts WHERE item_id = ?;");
        q.bind(1, itemId);
        q.exec();
        RefreshDraftCount();
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Draft remove failed: {}", ex.what());
    }
}

std::vector<DNItem> CrashRecoveryDB::LoadDrafts(const std::function<const DNItem*(int32_t)>& lookup) {
    std::vector<DNItem> drafts;
    if (!m_initialized) return drafts;
    try {
        SQLite::Statement q(*m_db,
                            "SELECT item_id, name, name_template, name_param, description, desc_template, desc_param, rank, level, hero_level,"
                            " job, overlap, sell, durability, gear_score, flags, states, icon_index FROM drafts ORDER BY saved_at;");
        while (q.executeStep()) {
            const int32_t id = q.getColumn(0).getInt();
            const DNItem* base = lookup(id);
            if (!base) continue;

            DNItem item = *base;
            item.name = q.getColumn(1).getString();
            item.nameTemplate = q.getColumn(2).getString();
            item.nameParam = q.getColumn(3).getString();
            item.description = q.getColumn(4).getString();
            item.descTemplate = q.getColumn(5).getString();
            item.descParam = q.getColumn(6).getString();
            item.rank = q.getColumn(7).getInt();
            item.levelLimit = q.getColumn(8).getInt();
            item.heroLevelLimit = q.getColumn(9).getInt();
            item.jobClass = q.getColumn(10).getString();
            item.overlapCount = q.getColumn(11).getInt();
            item.sellAmount = q.getColumn(12).getInt();
            item.durability = q.getColumn(13).getInt();
            item.gearScore = q.getColumn(14).getInt();
            const int flags = q.getColumn(15).getInt();
            item.isCash = (flags & 1) != 0;
            item.ableStorage = (flags & 2) != 0;
            item.ableWStorage = (flags & 4) != 0;
            DecodeStates(q.getColumn(16).getString(), item);
            if (!q.getColumn(17).isNull()) {
                const int32_t savedIcon = q.getColumn(17).getInt();
                if (savedIcon >= 0) item.iconIndex = savedIcon;
            }
            drafts.push_back(std::move(item));
        }
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Draft load failed: {}", ex.what());
    }
    return drafts;
}

void CrashRecoveryDB::Clear() {
    if (!m_initialized) return;
    try {
        m_db->exec("DELETE FROM drafts;");
        m_draftCount = 0;
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Draft clear failed: {}", ex.what());
    }
}

void CrashRecoveryDB::RefreshDraftCount() noexcept {
    if (!m_initialized || !m_db) {
        m_draftCount = 0;
        return;
    }
    try {
        SQLite::Statement q(*m_db, "SELECT COUNT(*) FROM drafts;");
        m_draftCount = q.executeStep() ? static_cast<size_t>(q.getColumn(0).getInt64()) : 0;
    } catch (...) {
        m_draftCount = 0;
    }
}

void CrashRecoveryDB::SetSetting(std::string_view key, std::string_view value) {
    if (!m_initialized) return;
    try {
        SQLite::Statement q(*m_db, "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);");
        q.bind(1, std::string(key));
        q.bind(2, std::string(value));
        q.exec();
    } catch (const std::exception& ex) {
        LOG_WARNING(Channels::sqlite, "Failed to persist setting '{}': {}", key, ex.what());
    }
}

std::string CrashRecoveryDB::GetSetting(std::string_view key, std::string_view defaultValue) const {
    if (!m_initialized) return std::string(defaultValue);
    try {
        SQLite::Statement q(*m_db, "SELECT value FROM settings WHERE key = ?;");
        q.bind(1, std::string(key));
        if (q.executeStep()) {
            return q.getColumn(0).getString();
        }
    } catch (...) {
    }
    return std::string(defaultValue);
}

} // namespace DNItemMaker::Data
