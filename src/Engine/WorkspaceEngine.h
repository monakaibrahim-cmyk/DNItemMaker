#pragma once

/**
 * @file WorkspaceEngine.h
 * @brief High-level workspace orchestrator coordinating data, 3D assets, caching, and undo/redo.
 *
 * Encapsulates the runtime engine state between the low-level data layer (`ItemManager`,
 * `CrashRecoveryDB`) and the presentation UI (`CMainFrame`). Manages in-memory LRU caches
 * for DirectDraw Surface (DDS) textures and 3D meshes, multi-level transactional undo/redo
 * history, and batch automation pipelines.
 */

#include "Data/CrashRecoveryDB.h"
#include "Data/ItemManager.h"
#include "Graphics/DDSParser.h"
#include "Graphics/MeshParser.h"
#include "Graphics/MeshRenderer.h"
#include "Graphics/SkinParser.h"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace DNItemMaker::Engine {

/**
 * @brief Consolidated 3D asset bundle required to render an item in the preview viewport.
 */
struct ModelAssetBundle {
    bool hasModel = false;                          ///< True if both skin and mesh were located and loaded.
    std::filesystem::path skinPath;                 ///< Resolved `.skn` skin file path on disk.
    std::filesystem::path meshPath;                 ///< Resolved `.msh` mesh file path on disk.
    std::shared_ptr<const Graphics::ParsedSkin> skin; ///< Parsed skin definition with materials and texture names.
    std::shared_ptr<const Graphics::ParsedMesh> mesh; ///< Parsed 3D geometry vertices and index buffers.
    std::vector<Graphics::SubmeshMaterial> materials; ///< Submesh textures and shading properties.
    std::vector<std::string> textureNames;          ///< List of referenced DDS texture filenames.
    std::string statusSummary;                      ///< Diagnostic loading status for UI telemetry.
};

/**
 * @brief Configuration parameters for automated tier set generation (Normal through Ancient).
 */
struct TierSetConfig {
    int32_t sourceItemId = 0;                       ///< Base item template to generate rank tiers from.
    int targetTableIndex = -1;                      ///< Partition index (-1 for same table as source).
    std::string baseName;                           ///< Base display name for the series.
    std::array<bool, 7> includeRanks{ false, true, true, true, true, true, false }; ///< Ranks to generate (0..6).
    double statGrowthPerRankPct = 28.0;             ///< Percentage stat scaling increase per rank level.
    bool useAutoCurve = false;                      ///< When true, applies Dragon Nest official power curves.
    bool autoRecalcEconomy = true;                  ///< Automatically update gear score and sell value.
    bool autoRichTooltip = false;                   ///< Automatically generate rich color-coded tooltip.
};

/**
 * @brief Configuration parameters for automated class series generation (Warrior, Archer, etc.).
 */
struct ClassSeriesConfig {
    int32_t sourceItemId = 0;                       ///< Base item template to generate class series from.
    int targetTableIndex = -1;                      ///< Partition index (-1 for same table as source).
    std::string seriesNameTemplate;                 ///< Name template containing optional "{CLASS}" token.
    std::array<bool, 10> includeClasses{ true, true, true, true, true, true, true, true, true, true }; ///< Job classes 1..10.
    bool adaptPhysicalMagicalStats = true;          ///< Automatically convert attack stats based on class archetype.
    bool autoRichTooltip = false;                   ///< Automatically generate rich color-coded tooltip.
};

/**
 * @brief Specification for executing bulk edits across a collection of items.
 */
struct BatchEditSpec {
    bool scaleStats = false;                        ///< Scale numeric stat values.
    double statMultiplier = 1.0;                    ///< Multiplier factor (e.g. 1.25 for +25%).
    bool autoCurveStats = false;                    ///< Adjust stats to official rank/level curves.
    bool sanitizeStats = false;                     ///< Remove duplicate and empty stat slots.

    bool setRank = false;                           ///< Overwrite item rank.
    int32_t newRank = 0;                            ///< Target rank (0..6).

    bool setLevelLimit = false;                     ///< Overwrite required level limit.
    int32_t newLevelLimit = 1;                      ///< Target level (1..99).

    bool setHeroLevelLimit = false;                 ///< Overwrite hero level limit.
    int32_t newHeroLevelLimit = 0;                  ///< Target hero level.

    bool setOverlapCount = false;                   ///< Overwrite inventory stack limit.
    int32_t newOverlapCount = 1;                    ///< Target stack size.

    bool setCashFlag = false;                       ///< Overwrite cash-shop flag.
    bool cashValue = false;                         ///< New cash status.

    bool setStorageFlags = false;                   ///< Overwrite storage permission flags.
    bool ableStorage = true;                        ///< Personal storage allowed.
    bool ableWStorage = true;                       ///< Server warehouse storage allowed.

    bool autoRecalcEconomy = false;                 ///< Recalculate gear score and vendor sell amount.

    std::string namePrefix;                         ///< Text prefix prepended to display name.
    std::string nameSuffix;                         ///< Text suffix appended to display name.
    bool autoForkSharedMidsOnRename = true;         ///< Automatically assign unique MID if shared by others.
};

/**
 * @class WorkspaceEngine
 * @brief Central application controller coordinating data mutations, caching, and 3D preview.
 */
class WorkspaceEngine {
public:
    WorkspaceEngine();
    ~WorkspaceEngine();

    WorkspaceEngine(const WorkspaceEngine&) = delete;
    WorkspaceEngine& operator=(const WorkspaceEngine&) = delete;

    /**
     * @brief Initializes SQLite crash recovery database and persistent services.
     * @param appDirectory Directory where database and cache stores reside.
     */
    void InitializeServices(const std::filesystem::path& appDirectory);

    /**
     * @brief Auto-detects the default workspace path from saved settings or game directory.
     * @return Path to existing resource directory, or empty path if not found.
     */
    std::filesystem::path DetectInitialWorkspacePath() const;

    /**
     * @brief Opens and loads a Dragon Nest resource directory.
     * @param resourcePath Path to the Dragon Nest `resource` root folder.
     * @param outRecoveredDrafts Optional output receiving number of recovered SQLite drafts.
     * @param onModelIndexReady Optional callback invoked when background model indexing finishes.
     * @param onProgress Optional callback receiving real-time progress updates.
     * @return true on success, false on load error.
     */
    bool OpenWorkspace(const std::filesystem::path& resourcePath,
                       size_t* outRecoveredDrafts = nullptr,
                       std::function<void()> onModelIndexReady = {},
                       Data::LoadProgressCallback onProgress = {});

    /** @brief Returns true if an active workspace has been loaded. */
    bool IsWorkspaceLoaded() const { return m_items.GetItemCount() > 0; }

    /** @brief Returns the filesystem path to the currently open workspace resource directory. */
    const std::filesystem::path& ResourcePath() const noexcept { return m_items.GetResourcePath(); }

    /** @brief Provides mutable access to the underlying ItemManager. */
    Data::ItemManager& Items() { return m_items; }

    /** @brief Provides const access to the underlying ItemManager. */
    const Data::ItemManager& Items() const { return m_items; }

    /** @brief Provides mutable access to the SQLite crash recovery database. */
    Data::CrashRecoveryDB& RecoveryDB() { return m_recoveryDb; }

    /** @brief Provides const access to the SQLite crash recovery database. */
    const Data::CrashRecoveryDB& RecoveryDB() const { return m_recoveryDb; }

    // --- Undo / Redo Transaction Management ---

    /** @brief Returns true if there are actions available to undo on the stack. */
    bool CanUndo() const noexcept { return !m_undoStack.empty(); }

    /** @brief Returns the total number of undo transactions currently recorded. */
    size_t GetUndoCount() const noexcept { return m_undoStack.size(); }

    /** @brief Returns the human-readable action label of the most recent undo transaction. */
    std::string GetUndoLabel() const;

    /** @brief Returns a list of all transaction labels currently on the undo stack. */
    std::vector<std::string> GetUndoHistoryLabels() const;

    /**
     * @brief Reverts the most recent operation, restoring previous item snapshots.
     * @param outSummary Diagnostic summary of restored items.
     * @return true on successful revert, false if undo stack was empty.
     */
    bool UndoLastAction(std::string& outSummary);

    // --- Item Mutations with Undo Tracking and Recovery Journaling ---

    /**
     * @brief Applies modifications to an item, recording an undo step and updating SQLite WAL.
     * @param edited Modified item data.
     * @return true if successfully applied, false on validation failure.
     */
    bool ApplyItemEdit(const Data::DNItem& edited);

    /**
     * @brief Clones an item, recording an undo step and updating SQLite WAL.
     * @param sourceId Source item to clone.
     * @param newName New display name.
     * @param targetTableIndex Target partition index (-1 for same table as source).
     * @param requestedNewId Optional explicit ID (0 for auto-suggestion).
     * @return Newly created Item ID, or 0 on failure.
     */
    int32_t DuplicateItem(int32_t sourceId, const std::string& newName, int targetTableIndex = -1, int32_t requestedNewId = 0);

    /**
     * @brief Creates a new item from scratch, recording an undo step and updating SQLite WAL.
     * @param targetTableIndex Target partition index.
     * @param rawType Engine item type enum.
     * @param equipSubType Equipment slot subtype.
     * @param newName Display name.
     * @param requestedNewId Optional explicit ID (0 for auto-suggestion).
     * @return Newly created Item ID, or 0 on failure.
     */
    int32_t CreateNewItem(int targetTableIndex, int32_t rawType, int32_t equipSubType,
                          const std::string& newName, int32_t requestedNewId = 0);

    /**
     * @brief Deletes an item, recording an undo step and updating SQLite WAL.
     * @param id Item ID to delete.
     * @return true on success, false if item does not exist.
     */
    bool DeleteItem(int32_t id);

    /**
     * @brief Directly modifies a raw DNT column cell with undo support.
     * @param itemId Target item ID.
     * @param detailTable If true, modifies joined detail table instead of main item table.
     * @param colIdx 0-based column index.
     * @param textValue New string representation.
     * @return true on success, false on failure.
     */
    bool SetRawItemCell(int32_t itemId, bool detailTable, size_t colIdx, std::string_view textValue);

    // --- Studio Automation Workflows ---

    /**
     * @brief Forks shared string Message IDs so edits do not affect other items.
     * @param itemId Target item ID.
     * @param forkName If true, allocates fresh MID for item name.
     * @param forkDesc If true, allocates fresh MID for item tooltip.
     * @return true if one or more MIDs were forked.
     */
    bool ForkExclusiveMids(int32_t itemId, bool forkName = true, bool forkDesc = true);

    /**
     * @brief Copies 2D icon and 3D visual skin references from a source item with undo support.
     * @param targetId Target item to receive visuals.
     * @param sourceId Source item providing visuals.
     * @param copyIcon If true, copies `_IconImageIndex`.
     * @param copy3DSkin If true, copies 3D skin model associations.
     * @return true on success, false on failure.
     */
    bool CopyVisualFromItem(int32_t targetId, int32_t sourceId, bool copyIcon = true, bool copy3DSkin = true);

    /**
     * @brief Clones equipment detail row (weapontable or partstable) with undo support.
     * @param targetId Target item to receive cloned detail.
     * @param sourceId Source item to clone from.
     * @return true on success, false on failure.
     */
    bool CloneDetailRowFromItem(int32_t targetId, int32_t sourceId);

    /**
     * @brief Generates a complete progression set of items across ranks (Normal..Ancient).
     * @param cfg Tier set generation settings.
     * @param outSummary Diagnostic summary report.
     * @return List of newly created Item IDs.
     */
    std::vector<int32_t> GenerateRankTierSet(const TierSetConfig& cfg, std::string& outSummary);

    /**
     * @brief Generates matching equipment for multiple job classes with stat adaptations.
     * @param cfg Class series generation settings.
     * @param outSummary Diagnostic summary report.
     * @return List of newly created Item IDs.
     */
    std::vector<int32_t> GenerateClassEquipmentSeries(const ClassSeriesConfig& cfg, std::string& outSummary);

    /**
     * @brief Applies bulk edits to a collection of items in a single atomic undo transaction.
     * @param itemIds List of item IDs to modify.
     * @param spec Batch edit modifications to apply.
     * @param outSummary Diagnostic summary of changes.
     * @return Count of successfully modified items.
     */
    size_t ExecuteBatchEdit(const std::vector<int32_t>& itemIds, const BatchEditSpec& spec, std::string& outSummary);

    /**
     * @brief Imports items from JSON into a target partition with undo tracking.
     * @param jsonText JSON text payload.
     * @param targetTableIndex Target partition index.
     * @param outCreatedIds List receiving newly created item IDs.
     * @param outSummary Diagnostic import report.
     * @return Count of successfully imported items.
     */
    size_t ImportItemsJson(std::string_view jsonText, int targetTableIndex,
                           std::vector<int32_t>& outCreatedIds, std::string& outSummary);

    /**
     * @brief Commits all modified tables and strings back to disk, clearing SQLite WAL drafts.
     * @param report Diagnostic report detailing files saved and elapsed time.
     * @return true if all files were successfully written, false on any error.
     */
    bool CommitAllToDisk(std::string& report);

    /**
     * @brief Exports modified tables and strings into a standalone output directory.
     * @param outputFolder Target directory path.
     * @param report Diagnostic report.
     * @return true on success, false on I/O error.
     */
    bool SaveModifiedToFolder(const std::filesystem::path& outputFolder, std::string& report);

    /** @brief Discards all uncommitted drafts stored in the SQLite recovery database. */
    void DiscardRecoveredDrafts();

    // --- 2D Icon Atlas & In-Memory Asset Caching ---

    /**
     * @brief Retrieves a cropped 50x50 icon image for an icon index from cached DDS atlas pages.
     * @param iconIndex 0-based icon index.
     * @param outTile Destination image buffer receiving RGBA pixel data.
     * @return true if successfully extracted, false if index or file is missing.
     */
    bool GetItemIcon(int32_t iconIndex, Graphics::ImageBuffer& outTile);

    /**
     * @brief Retrieves a full decoded DDS atlas page from the cache.
     * @param pageNumber 1-based page number (e.g. 1 for ItemIcon.dds, 2 for ItemIcon02.dds).
     * @return Shared pointer to const ImageBuffer, or nullptr.
     */
    std::shared_ptr<const Graphics::ImageBuffer> GetIconAtlasPage(int pageNumber);

    /** @brief Returns the highest discovered icon atlas page number. */
    int GetMaxIconAtlasPage() const { return m_maxIconPage; }

    /** @brief Evicts all in-memory decoded textures, atlas sheets, skins, and meshes. */
    void ClearNativeCaches();

    // --- 3D Model Asset Resolution ---

    /**
     * @brief Resolves and loads the 3D skin, mesh, textures, and materials for an item.
     * @param item Item to resolve 3D geometry for.
     * @return Populated ModelAssetBundle structure.
     */
    ModelAssetBundle LoadModelForItem(const Data::DNItem& item);

private:
    struct ItemUndoRecord {
        int32_t id = 0;
        bool existedBefore = true;
        bool wasModifiedBefore = false;
        Data::DNItem snapshot;
    };

    struct UndoEntry {
        std::string label;
        std::vector<ItemUndoRecord> records;
    };

    void PushUndoEntry(UndoEntry entry);
    ItemUndoRecord CaptureExistingSnapshot(int32_t id) const;

    std::shared_ptr<const Graphics::ImageBuffer> LoadTextureCached(const std::filesystem::path& ddsPath);
    std::shared_ptr<const Graphics::ParsedSkin> LoadSkinCached(const std::filesystem::path& skinPath);
    std::shared_ptr<const Graphics::ParsedMesh> LoadMeshCached(const std::filesystem::path& meshPath);
    void ScanIconPageCount();

    Data::ItemManager m_items;
    Data::CrashRecoveryDB m_recoveryDb;

    std::deque<UndoEntry> m_undoStack;

    std::mutex m_assetMutex;
    std::unordered_map<int32_t, Graphics::ImageBuffer> m_iconTileCache;
    std::unordered_map<int, std::shared_ptr<const Graphics::ImageBuffer>> m_atlasCache;
    std::unordered_map<std::string, std::shared_ptr<const Graphics::ImageBuffer>> m_textureCache;
    std::unordered_map<std::string, std::shared_ptr<const Graphics::ParsedSkin>> m_skinCache;
    std::unordered_map<std::string, std::shared_ptr<const Graphics::ParsedMesh>> m_meshCache;
    std::string m_resourceCacheTag;
    int m_maxIconPage = 1;

    std::jthread m_indexWorker;
};

} // namespace DNItemMaker::Engine
