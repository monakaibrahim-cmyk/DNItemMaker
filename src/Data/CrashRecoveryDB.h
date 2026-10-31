#pragma once

/**
 * @file CrashRecoveryDB.h
 * @brief Real-time SQLite Write-Ahead Logging (WAL) crash recovery journal.
 *
 * Persists uncommitted item modifications and workspace configuration state to a
 * local SQLite database in real time. If the editor terminates unexpectedly or encounters
 * a power failure, modified drafts are detected on the next startup and safely offered
 * for restoration.
 */

#include "ItemManager.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SQLite {
class Database;
}

namespace DNItemMaker::Data {

/**
 * @class CrashRecoveryDB
 * @brief Manages the local SQLite database for session recovery drafts and settings.
 *
 * Configured with `PRAGMA synchronous = NORMAL` and WAL journal mode for sub-millisecond
 * atomic transactions without blocking the user interface.
 */
class CrashRecoveryDB {
public:
    CrashRecoveryDB();
    ~CrashRecoveryDB();

    /**
     * @brief Opens or creates the SQLite recovery database file.
     * @param dbPath Destination database filesystem path (e.g. `Recovery.db`).
     * @return true on success, false on database failure.
     */
    bool Initialize(const std::filesystem::path& dbPath);

    /**
     * @brief Writes or updates an item draft record in the recovery journal.
     * @param item Item to persist as an active draft.
     * @return true if written successfully, false on error.
     */
    bool SaveDraft(const DNItem& item);

    /**
     * @brief Atomically writes multiple item drafts in a single database transaction.
     * @param items List of item pointers to persist.
     * @return true on success, false on failure.
     */
    bool SaveDraftsBatch(const std::vector<const DNItem*>& items);

    /**
     * @brief Removes a single item draft from the recovery journal after commit.
     * @param itemId Item ID to purge from recovery.
     */
    void RemoveDraft(int32_t itemId);

    /**
     * @brief Reads all stored draft items from the database.
     * @param lookup Resolver callback to find existing baseline item definitions by ID.
     * @return List of reconstructed DNItem instances with drafted changes applied.
     */
    std::vector<DNItem> LoadDrafts(const std::function<const DNItem*(int32_t)>& lookup);

    /**
     * @brief Clears all stored recovery drafts from the database.
     */
    void Clear();

    /** @brief Returns the total count of uncommitted drafts currently in the database. */
    size_t DraftCount() const noexcept { return m_draftCount; }

    /** @brief Returns true if the recovery database is initialized and operational. */
    bool IsReady() const noexcept { return m_initialized; }

    /**
     * @brief Sets a persistent key-value configuration string in the settings table.
     * @param key Setting key identifier.
     * @param value Setting string value.
     */
    void SetSetting(std::string_view key, std::string_view value);

    /**
     * @brief Retrieves a persistent configuration string from the settings table.
     * @param key Setting key identifier.
     * @param defaultValue Fallback value returned if the key is not found.
     * @return Resolved string value.
     */
    std::string GetSetting(std::string_view key, std::string_view defaultValue = {}) const;

private:
    void RefreshDraftCount() noexcept;

    std::unique_ptr<SQLite::Database> m_db;
    size_t m_draftCount = 0;
    bool m_initialized = false;
};

} // namespace DNItemMaker::Data
