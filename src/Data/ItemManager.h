#pragma once

/**
 * @file ItemManager.h
 * @brief Unified Dragon Nest item database manager across 60+ tables and string resources.
 *
 * Provides a consolidated, thread-safe in-memory database linking all discovered
 * `itemtable*.dnt` partitions with equipment detail tables (`weapontable*.dnt`,
 * `partstable*.dnt`, `partsskincombine.dnt`, `filetable.dnt`, `jobtable.dnt`) and
 * localized text entries (`uistring.xml`).
 */

#include "DNTReader.h"
#include "UIStringManager.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DNItemMaker::Data {

/**
 * @brief Coarse UI classification category derived from the item's raw type and equipment slot.
 */
enum class ItemGroup {
    Weapon = 0,    ///< Weapons, off-hands, secondary armaments.
    Armor = 1,     ///< Armor pieces (helm, body, legs, gloves, boots).
    Accessory = 2, ///< Rings, earrings, necklaces, talismans, crests.
    Other = 3      ///< Consumables, materials, cash items, quest items, misc.
};

/**
 * @brief Sortable columns supported by ItemManager search and list view queries.
 */
enum class ItemSortColumn : uint8_t {
    Id = 0,    ///< Sort by 32-bit primary key (Item ID).
    Name = 1,  ///< Sort by resolved localized display name.
    Rank = 2,  ///< Sort by item grade (Normal to Ancient).
    Level = 3, ///< Sort by required character level limit.
    Type = 4,  ///< Sort by raw game item type enum.
    Table = 5  ///< Sort by source DNT partition filename.
};

/**
 * @brief Canonical combat stat distribution presets for automated generation.
 */
enum class StatArchetype : uint8_t {
    PhysicalDps = 0,     ///< Attack power, agility/strength, physical critical.
    MagicalDps = 1,      ///< Magic attack power, intelligence, magic critical.
    TankVitality = 2,    ///< Max HP, vitality, defense, magic defense.
    CritFinalDamage = 3, ///< Critical strike, critical damage, final damage.
    ElementalHybrid = 4, ///< Fire, water, light, and dark elemental attack attributes.
    BalancedAllStat = 5  ///< Uniform split across primary attributes and defenses.
};

/**
 * @brief Represents one of ten possible stat modifier slots (_State1 .. _State10).
 */
struct StateSlot {
    int32_t type = -1;       ///< Stat identifier matching CDnItem::CalcStateValue, or -1 if unused.
    float genProb = 0.0f;    ///< Generation probability weight (0.0 to 1.0).
    std::string minValue = "0"; ///< Minimum value range bound.
    std::string maxValue = "0"; ///< Maximum value range bound.
};

/// Fixed number of stat modifier slots supported per item row by the Dragon Nest engine.
constexpr size_t kStateSlotCount = 10;

/**
 * @brief Comprehensive in-memory representation of a Dragon Nest game item.
 *
 * Joins attributes across `itemtable*.dnt`, equipment detail tables (`weapontable*.dnt` or
 * `partstable*.dnt`), and `uistring.xml` into a single high-performance structure.
 */
struct DNItem {
    int32_t id = 0;             ///< Unique primary key identifier (_ItemID).
    uint16_t tableIndex = 0;    ///< Index of the owning `itemtable*.dnt` partition.
    bool deleted = false;       ///< Flagged as deleted in the current editing session.

    uint32_t nameId = 0;        ///< Message ID reference in `uistring.xml` for the item name.
    uint32_t descId = 0;        ///< Message ID reference in `uistring.xml` for the item tooltip.
    std::string nameParam;      ///< Parameter list for name string formatting.
    std::string descParam;      ///< Parameter list for description string formatting.
    std::string nameTemplate;   ///< Raw unformatted template string from `uistring.xml`.
    std::string descTemplate;   ///< Raw unformatted description template.
    std::string name;           ///< Fully resolved localized display name.
    std::string description;    ///< Fully resolved localized description text.

    int32_t rawType = 0;        ///< Raw engine enum value (eItemTypeEnum).
    ItemGroup group = ItemGroup::Other; ///< Coarse UI classification.
    int32_t equipSubType = -1;  ///< Equipment slot subtype (weapontable `_EquipType` or partstable `_Parts`).

    int32_t rank = 0;           ///< Item grade/rank (0=Normal, 1=Magic, 2=Rare, 3=Epic, 4=Unique, 5=Legend, 6=Ancient).
    int32_t levelLimit = 1;     ///< Minimum character level required to equip or use.
    int32_t heroLevelLimit = 0; ///< Minimum hero/awakening level required.
    std::string jobClass = "0"; ///< Semicolon-delimited class IDs allowed, or "0" for all classes.
    int32_t overlapCount = 1;   ///< Maximum stack size in inventory.
    int32_t sellAmount = 0;     ///< NPC vendor sell value in copper coins.
    int32_t iconIndex = 0;      ///< 0-based icon index in the 2D DDS UI atlas sheets.
    int32_t gearScore = 0;      ///< Gear score rating for matchmaking and item level calculations.
    int32_t durability = 0;     ///< Maximum durability points (from equipment detail table).
    bool isCash = false;        ///< True if this is a cash-shop / premium item.
    bool ableStorage = true;    ///< True if storable in personal storage.
    bool ableWStorage = true;   ///< True if storable in shared server warehouse.

    int32_t dragonJewelType = 0;      ///< Socket configuration ID (_dragonjeweltype in itemtable col 103).
    int32_t enchantId = 0;            ///< Enhancement curve ID (_EnchantID in itemtable col 81).
    std::array<int32_t, 10> potentialIds{}; ///< Potential group IDs (_Potential1..10 in itemtable cols 8..17).
    int32_t setItemId = 0;            ///< Set item group ID (_SetItemID from weapontable/partstable).
    uint32_t setItemTextId = 0;       ///< Set name Message ID (_SetItemTextID from weapontable/partstable).

    std::array<StateSlot, kStateSlotCount> states; ///< 10 stat modifier slots.

    int32_t typeParam1 = 0;     ///< Auxiliary type parameter (e.g. Charm / Box Drop Set ID).
    int32_t typeParam2 = 0;     ///< Secondary type parameter.
    int32_t typeParam3 = 0;     ///< Tertiary type parameter.

    std::string lowerName;      ///< Lowercase cached display name for accelerated substring search.
};

/**
 * @brief Represents a single drop reward entry in a Dragon Nest charm / gacha box table.
 */
struct CharmReward {
    int32_t rowId = 0;              ///< DNT row ID in `charmitemtable*.dnt`.
    int32_t charmNum = 0;           ///< Associated charm group number (_CharmNum).
    int32_t itemId = 0;             ///< Reward item identifier (_ItemID).
    int32_t count = 1;              ///< Reward stack quantity (_Count).
    int32_t probWeight = 0;         ///< Generation probability weight from `charmitemprobtable*.dnt`.
    int32_t potenProb = 0;          ///< Potential probability factor (_PotenProb).
    int32_t period = 0;             ///< Expiration duration in seconds/days (_Period).
    int32_t gold = 0;               ///< Direct gold coin reward (_Gold).
    bool broadcast = false;         ///< True if server-wide announcement is triggered on drop (_Msg).
    int32_t msgType = 0;            ///< Chat announcement broadcast format identifier (_MsgType).
    std::string itemName;           ///< Localized display name resolved from UI strings.
    int32_t itemRank = 0;           ///< Reward item grade (0=Normal .. 6=Ancient).
    int32_t itemIconIndex = 0;      ///< 0-based icon index in 2D DDS UI atlas.
    double dropRatePercent = 0.0;   ///< Normalized drop probability percentage (0.0% to 100.0%).
    double cumulativePercent = 0.0; ///< Upper cumulative probability threshold for RNG simulation.
    std::string sourceTable;        ///< Name of the source DNT table file.
    size_t tableIndex = 0;          ///< Internal index of the owning charm table partition.
};

/**
 * @brief Represents a consolidated drop group / reward pool indexed by _CharmNum.
 */
struct CharmGroup {
    int32_t charmNum = 0;           ///< Charm group identifier (_CharmNum).
    std::string sourceTable;        ///< Primary source table filename.
    int64_t totalWeight = 0;        ///< Sum of all individual probability weights in this set.
    std::vector<CharmReward> rewards; ///< List of possible drop rewards.
};

/**
 * @brief Represents key unlock requirements from `charmitemkeytable.dnt`.
 */
struct CharmKeyInfo {
    int32_t charmItemId = 0;                           ///< Target charm or box item ID.
    std::array<int32_t, 5> keys{ 0, 0, 0, 0, 0 };      ///< Up to 5 key item IDs required to open.
    std::array<int32_t, 5> keyCosts{ 0, 0, 0, 0, 0 };  ///< Quantity of each key required.
};

/**
 * @brief Dragon Jewel / Jade item details from `dragonjeweltable.dnt`.
 */
struct DragonJewelInfo {
    int32_t itemId = 0;              ///< Item ID (_ItemID, matching primary key).
    int32_t type = 0;                ///< Jewel category: 1 = Offensive / Attack, 2 = Defensive / Armor, 3 = Skill / Special.
    int32_t period = 0;              ///< Duration / period.
    int32_t applicableSlots = 0;     ///< Slot bitmask for weapons / armor parts.
    int32_t applicableSlots2 = 0;    ///< Secondary slot bitmask.
    bool canApplySealedItem = false; ///< True if jade can be inserted into sealed equipment.
    int32_t charge = 0;              ///< Charge stat value.
    int32_t elementalType = -1;      ///< Elemental affinity: -1 = None/Phys, 0 = Water, 1 = Fire, 2 = Light, 3 = Dark.

    std::string GetTypeName() const;
    std::string GetElementName() const;
    std::vector<std::string> GetApplicableSlotNames() const;
};

/**
 * @brief Equipment socket configuration from `dragonjewelslottable.dnt`.
 */
struct DragonJewelSlotConfig {
    int32_t jewelTypeId = 0;         ///< Socket configuration ID (_DragonJewelID).
    float ratio = 0.0f;              ///< Ratio multiplier (_DragonJewelRatio).
    std::array<int32_t, 4> slots{};  ///< Sockets 1..4 (1 = Attack, 2 = Defense, 3 = Skill, 0 = None).
    bool isEnchanted = false;        ///< Requires enhancement to unlock sockets.

    int GetActiveSlotCount() const noexcept;
    std::string GetSummary() const;
};

/**
 * @brief Represents a set item piece bonus tier from `setitemtable.dnt` / `setitemtable_cash.dnt`.
 */
struct SetItemBonus {
    int32_t needNum = 0;             ///< Number of set pieces required (e.g. 2, 3, 4, 5, 6, 7).
    int32_t stateType = 0;           ///< State type enum (e.g. Strength, Max HP, Final Damage).
    std::string stateValue;          ///< State bonus value (e.g. "500", "0.05").

    std::string GetFormattedBonus() const;
};

/**
 * @brief Represents a set skill bonus tier from `setitemtable.dnt`.
 */
struct SetSkillBonus {
    int32_t needNum = 0;             ///< Pieces required.
    int32_t skillId = 0;             ///< Triggered / boosted skill ID.
    int32_t skillLevel = 0;          ///< Skill level bonus.
};

/**
 * @brief Complete Set Item definition with member pieces and tiered bonuses.
 */
struct SetItemGroup {
    int32_t setItemId = 0;           ///< Unique set identifier (_SetItemID).
    uint32_t setNameMid = 0;         ///< Message ID from _SetItemTextID.
    std::string setName;             ///< Resolved set name from uistring.xml.
    std::vector<SetItemBonus> bonuses;      ///< Stat bonuses per piece tier.
    std::vector<SetSkillBonus> skillBonuses;///< Skill bonuses.
    std::vector<int32_t> memberItemIds;     ///< List of all equipment item IDs belonging to this set.
    int32_t transformItemId = 0;     ///< Costume transformation effect item ID.
    int32_t transformPartsNum = 0;   ///< Parts required for transformation.
};

/**
 * @brief Single enhancement level step (+1..+20) from `enchanttable*.dnt`.
 */
struct EnchantStep {
    int32_t enchantId = 0;           ///< Enchant sequence identifier (_EnchantID).
    int32_t level = 0;               ///< Upgrade level (1..20).
    float successRatio = 0.0f;       ///< Success probability (0.0 to 1.0).
    float breakRatio = 0.0f;         ///< Destruction probability (0.0 to 1.0).
    int32_t minDown = 0;             ///< Minimum level downgrade on failure.
    int32_t maxDown = 0;             ///< Maximum level downgrade on failure.
    int64_t needCoin = 0;            ///< Gold cost in copper coins.
    std::array<int32_t, 5> needItemIds{};   ///< Up to 5 crafting material item IDs.
    std::array<int32_t, 5> needItemCounts{};///< Required quantities.
    struct StatBonus {
        int32_t stateType = 0;
        std::string stateValue;
    };
    std::vector<StatBonus> stats;    ///< Stat boost granted at this level.
    int32_t protectItemCount = 0;    ///< Protection jelly count required.
    int32_t gearScore = 0;           ///< Gear score increment.
};

/**
 * @brief Full enhancement curve (+1..+20) for an item group.
 */
struct EnchantCurve {
    int32_t enchantId = 0;
    std::vector<EnchantStep> steps;  ///< Steps sorted by level.
};

/**
 * @brief Potential variant from `potentialtable*.dnt`.
 */
struct PotentialVariant {
    int32_t potentialId = 0;         ///< Potential group ID.
    int32_t potentialNo = 0;         ///< Variant sequence index.
    float ratio = 0.0f;              ///< Generation probability ratio.
    uint32_t prefixMid = 0;          ///< Message ID for name prefix (e.g. Destruction, Bear).
    std::string prefixName;          ///< Resolved prefix string.
    std::vector<std::pair<int32_t, std::string>> stats; ///< Granted stats.
};

/**
 * @brief Potential group holding all possible variants for a potential ID.
 */
struct PotentialGroup {
    int32_t potentialId = 0;
    std::vector<PotentialVariant> variants;
};

/**
 * @brief Filter parameters for executing search queries over the item database.
 */
struct ItemSearchFilter {
    std::string keyword;        ///< Substring query matched against name and Item ID.
    int groupFilter = -1;       ///< -1 for all groups, or 0..3 for specific ItemGroup.
    int tableFilter = -1;       ///< -1 for all tables, or >= 0 for a specific table index.
    int rankFilter = -1;        ///< -1 for all ranks, or 0..6 for specific rank.
    bool modifiedOnly = false;  ///< When true, only returns items created or edited in this session.
    ItemSortColumn sortBy = ItemSortColumn::Id; ///< Column to sort results by.
    bool sortAscending = true;  ///< True for ascending order, false for descending.
    size_t limit = 0;           ///< Maximum number of matches to return (0 = unlimited).
};

/**
 * @brief Search query result containing matched item handles and total counts.
 */
struct ItemQueryResult {
    std::vector<const DNItem*> items; ///< Pointers to matching items in memory.
    size_t totalMatches = 0;          ///< Total matching items before limit truncation.
};

/**
 * @brief Validation and integrity report generated by ItemManager::RunWorkspaceAudit.
 */
struct WorkspaceAuditReport {
    size_t totalChecked = 0;          ///< Total items scanned during the audit.
    size_t emptyOrMissingNames = 0;   ///< Items with unresolved or empty localized names.
    size_t missingEquipDetailRows = 0;///< Weapons or armors lacking corresponding detail rows.
    size_t invalidStatRanges = 0;     ///< Stat slots where min value exceeds max value.
    size_t sharedNameMids = 0;        ///< Items sharing the same `_NameID` with another item.
    std::string reportText;           ///< Human-readable diagnostic breakdown.
};

/**
 * @typedef LoadProgressCallback
 * @brief Real-time progress reporter invoked during workspace loading.
 * @param progress Normalized progress from 0.0f to 1.0f.
 * @param stage High-level loading stage description.
 * @param detail Specific file or table currently being processed.
 */
using LoadProgressCallback = std::function<void(float progress, std::string_view stage, std::string_view detail)>;

/**
 * @class ItemManager
 * @brief Central controller for loading, indexing, mutating, and validating Dragon Nest items.
 *
 * Manages all loaded DNT partitions, indexes items into a pointer-stable deque, handles
 * bidirectional text resolution with `uistring.xml`, tracks modified item IDs, and provides
 * studio-grade automation routines (archetypes, stat scaling, tooltip generation, audits).
 */
class ItemManager {
public:
    ItemManager();

    /**
     * @brief Discovers and loads all item, equipment, string, and model tables from a resource folder.
     * @param resourcePath Path to the Dragon Nest `resource` root directory.
     * @param onProgress Optional callback receiving real-time progress updates.
     * @return true if successfully loaded, false on fatal failure.
     */
    bool LoadWorkspace(const std::filesystem::path& resourcePath, LoadProgressCallback onProgress = nullptr);

    /**
     * @brief Retrieves a pointer to an item by its ID.
     * @param id 32-bit Item ID.
     * @return Pointer to const DNItem if found, or nullptr.
     */
    const DNItem* GetItem(int32_t id) const;

    /**
     * @brief Executes a structured search query using an ItemSearchFilter.
     * @param filter Search criteria and sorting options.
     * @return ItemQueryResult with matched items.
     */
    ItemQueryResult Search(const ItemSearchFilter& filter) const;

    /**
     * @brief Convenience overload for keyword and group filtering.
     * @param keyword Name or ID substring query.
     * @param groupFilter ItemGroup index (-1 for all).
     * @param limit Maximum results to return.
     * @return ItemQueryResult with matched items.
     */
    ItemQueryResult Search(const std::string& keyword, int groupFilter, size_t limit) const;

    /**
     * @brief Applies modifications to an item, syncing in-memory DNT tables and UI strings.
     * @param item Modified item data.
     * @return true if successfully applied, false on validation failure.
     */
    bool ApplyItem(const DNItem& item);

    /**
     * @brief Clones an existing item into a target table partition.
     * @param sourceId ID of the source item to duplicate.
     * @param newName New display name for the cloned item.
     * @param targetTableIndex Target partition index (-1 for same table as source).
     * @param requestedNewId Optional specific ID (0 for auto-suggested next ID).
     * @return Newly assigned Item ID, or 0 on failure.
     */
    int32_t DuplicateItem(int32_t sourceId, const std::string& newName, int targetTableIndex = -1, int32_t requestedNewId = 0);

    /**
     * @brief Creates a new item row from scratch with appropriate detail rows.
     * @param targetTableIndex Partition index where the item will be added.
     * @param rawType Engine item type enum (eItemTypeEnum).
     * @param equipSubType Equipment slot subtype, or -1 for non-equipment.
     * @param newName Display name for the new item.
     * @param requestedNewId Optional specific ID (0 for auto-suggested next ID).
     * @return Newly assigned Item ID, or 0 on failure.
     */
    int32_t CreateNewItem(int targetTableIndex, int32_t rawType, int32_t equipSubType,
                          const std::string& newName, int32_t requestedNewId = 0);

    /**
     * @brief Flags an item as deleted and removes its joined detail row.
     * @param id Item ID to delete.
     * @return true on success, false if item does not exist.
     */
    bool DeleteItem(int32_t id);

    /**
     * @brief Directly edits a single raw column cell in the underlying DNT table.
     * @param itemId Target item ID.
     * @param detailTable If true, modifies joined weapon/parts detail table instead of main item table.
     * @param colIdx 0-based column index.
     * @param textValue New string representation of the value.
     * @return true on success, false on failure.
     */
    bool SetRawItemCell(int32_t itemId, bool detailTable, size_t colIdx, std::string_view textValue);

    /**
     * @brief Copies visual attributes (2D icon and 3D `.skn` skin references) from a source item.
     * @param targetId Item to receive the visual properties.
     * @param sourceVisualItemId Source item providing the visuals.
     * @param copyIcon If true, copies `_IconImageIndex`.
     * @param copy3DSkin If true, copies weapon/parts 3D skin associations.
     * @return true on success, false on failure.
     */
    bool CopyVisualFromItem(int32_t targetId, int32_t sourceVisualItemId, bool copyIcon = true, bool copy3DSkin = true);

    /**
     * @brief Clones the entire detail row (weapontable or partstable) from a source item.
     * @param targetId Target item to receive cloned detail.
     * @param sourceItemId Source item to clone from.
     * @return true on success, false on failure.
     */
    bool CloneDetailRowFromItem(int32_t targetId, int32_t sourceItemId);

    /**
     * @brief Allocates an unused Message ID in `uistring.xml` and initializes its text.
     * @param initialText Initial text content.
     * @return Newly allocated Message ID.
     */
    uint32_t AllocateUIStringMid(const std::string& initialText);

    /**
     * @brief Counts how many active items reference a given string Message ID.
     * @param mid Message ID to check.
     * @return Number of active item name and description references.
     */
    size_t CountItemsUsingMid(uint32_t mid) const noexcept;

    /** @brief Checks if an item has been modified during this session. */
    bool IsItemModified(int32_t id) const noexcept { return m_modifiedItemIds.contains(id); }

    /** @brief Returns the count of items modified during this session. */
    size_t GetModifiedItemCount() const noexcept { return m_modifiedItemIds.size(); }

    /** @brief Returns a list of all item IDs modified in this session. */
    std::vector<int32_t> GetModifiedItemIds() const;

    /** @brief Sets or clears the modified flag for a specific item. */
    void SetItemModifiedFlag(int32_t id, bool modified);

    /**
     * @brief Suggests an available, unused item ID.
     * @param tableIndex Optional table partition index to search near (-1 for any).
     * @param nearId Optional baseline ID to search near (0 for table maximum).
     * @return Available unused 32-bit item ID.
     */
    int32_t SuggestNextItemId(int tableIndex = -1, int32_t nearId = 0) const;

    // --- Studio Automation, Scaling, and Economy Calculations ---

    /** @brief Computes recommended gear score based on level limit, rank, and stat rolls. */
    static int32_t ComputeSuggestedGearScore(const DNItem& item) noexcept;

    /** @brief Computes recommended NPC vendor sell amount based on rank and level. */
    static int32_t ComputeSuggestedSellAmount(const DNItem& item) noexcept;

    /** @brief Computes standard equipment durability based on slot and level. */
    static int32_t ComputeSuggestedDurability(const DNItem& item) noexcept;

    /** @brief Configures stat slots according to a standard combat archetype. */
    static void ApplyStatArchetype(DNItem& item, StatArchetype archetype);

    /** @brief Automatically adjusts existing stat values along official level and rank power curves. */
    static void AutoScaleStatsByLevelAndRank(DNItem& item);

    /** @brief Multiplies all stat slot values by a custom multiplier with optional max variance. */
    static void ScaleItemStats(DNItem& item, double multiplier, bool spreadMax15Pct = false);

    /** @brief Removes duplicate, zero, or unassigned stat slots and shifts active ones forward. */
    static void CompactAndSanitizeStats(DNItem& item);

    /** @brief Generates formatted, color-coded BBCode tooltip description for the item. */
    std::string GenerateRichTooltipDescription(const DNItem& item) const;

    /** @brief Scans all loaded tables for structural inconsistencies and missing relations. */
    WorkspaceAuditReport RunWorkspaceAudit(size_t maxSamplesPerIssue = 25) const;

    /** @brief Serializes a list of items and their localized strings to JSON format. */
    std::string ExportItemsToJson(const std::vector<int32_t>& itemIds) const;

    /** @brief Imports item definitions from JSON text into a target table partition. */
    size_t ImportItemsFromJson(std::string_view jsonText, int targetTableIndex,
                               std::vector<int32_t>& outCreatedIds, std::string& outReport);

    /** @brief Writes all modified DNT tables and `uistring.xml` back to the active workspace directory. */
    bool CommitToDisk(std::string& report);

    /** @brief Exports only modified DNT tables and `uistring.xml` into a selected destination folder. */
    bool SaveModifiedToFolder(const std::filesystem::path& outputFolder, std::string& report);

    /** @brief Returns true if any loaded DNT table or string file has uncommitted changes. */
    bool HasUnsavedChanges() const noexcept;

    /** @brief Returns the count of dirty DNT tables awaiting disk commit. */
    size_t GetDirtyTableCount() const noexcept;

    /** @brief Triggers background indexing of 3D mesh, skin, and texture files. */
    void PrepareModelIndex() const { IndexModels(); }

    /** @brief Returns true if the background model index is ready for lookups. */
    bool IsModelIndexReady() const noexcept { return m_modelIndexBuilt.load(std::memory_order_acquire); }

    /** @brief Performs in-memory round-trip serialization verification across all tables. */
    bool VerifyRoundTrip(std::string& report) const;

    // --- Table Introspection ---

    /** @brief Total count of loaded `itemtable*.dnt` partitions. */
    size_t GetItemTableCount() const noexcept { return m_itemTables.size(); }

    /** @brief Retrieves the DNTTable instance for an item table partition. */
    const DNTTable& GetItemTable(size_t index) const noexcept { return m_itemTables[index]; }

    /** @brief Retrieves the filename of an item table partition. */
    const std::string& GetItemTableName(size_t index) const noexcept;

    /** @brief Finds the index of an item table partition by filename. */
    int FindItemTableByName(std::string_view fileName) const;

    /** @brief Retrieves the joined equipment detail table (weapon or parts) for an item. */
    const DNTTable* GetDetailTableForItem(const DNItem& item) const;

    // --- UI String and Visual Asset Resolution ---

    /** @brief Resolves a Message ID and formats it with parameter tokens. */
    std::string ResolveText(uint32_t mid, std::string_view param) const;

    /** @brief Applies parameter tokens to a template string. */
    std::string ResolveTemplate(std::string_view templateText, std::string_view param) const;

    /** @brief Converts semicolon-delimited job class IDs into human-readable class names. */
    std::string DescribeJobs(std::string_view rawJobList) const;

    /** @brief Resolves the filesystem path to the `.skn` skin file for an item. */
    std::filesystem::path FindSkinPath(const DNItem& item) const;

    /** @brief Resolves the filesystem path to the `.msh` mesh file for an item. */
    std::filesystem::path FindMeshPath(const DNItem& item) const;

    /** @brief Resolves a 3D model texture or sub-asset by filename near a relative path. */
    std::filesystem::path FindModelResource(const std::string& fileName, const std::filesystem::path& nearFile = {}) const;

    /** @brief Computes the filesystem path of the 2D DDS icon atlas page containing an icon index. */
    std::filesystem::path IconAtlasPath(int32_t iconIndex) const;

    /** @brief Returns the root resource directory path of the active workspace. */
    const std::filesystem::path& GetResourcePath() const noexcept { return m_resourcePath; }

    /** @brief Returns the total count of active items indexed in memory. */
    size_t GetItemCount() const noexcept { return m_indexById.size(); }

    /** @brief Returns the total count of string entries in `uistring.xml`. */
    size_t GetUIStringCount() const noexcept { return m_uiStrings.GetCount(); }

    /** @brief Provides const reference to the UIStringManager. */
    const UIStringManager& GetUIStrings() const noexcept { return m_uiStrings; }

    /** @brief Returns the elapsed time in milliseconds for the last workspace load. */
    double GetLastLoadMilliseconds() const noexcept { return m_lastLoadMs; }

    /** @brief Converts an item rank integer (0..6) to its display string (Normal..Ancient). */
    static const char* RankName(int32_t rank) noexcept;

    /** @brief Converts a raw item type integer to its descriptive name. */
    static std::string TypeName(int32_t rawType);

    /** @brief Converts an ItemGroup enum to its display name. */
    static const char* GroupName(ItemGroup group) noexcept;

    /** @brief Resolves the equipment subtype display name for an item. */
    static std::string_view EquipSubTypeName(const DNItem& item) noexcept;

    /** @brief Converts a state modifier type integer to its human-readable stat name. */
    static const char* StateDisplayName(int32_t stateType) noexcept;

    /** @brief Returns list of known engine item types with enum IDs and labels. */
    static const std::vector<std::pair<int32_t, const char*>>& KnownItemTypes();

    /** @brief Returns ordered list of human-readable stat type picker display names. */
    static const std::vector<std::string>& StateTypeNames();

    /** @brief Converts a 0-based picker index into an engine state type ID. */
    static int32_t StateTypeFromIndex(int index) noexcept;

    /** @brief Converts an engine state type ID to its 0-based picker index. */
    static int StateIndexFromType(int32_t type) noexcept;

    // --- Item Capabilities & Dynamic Tab Filters ---

    /** @brief Checks if an item has combat or modifier stats. */
    static bool HasStats(const DNItem& item) noexcept;

    /** @brief Checks if an item possesses an associated 3D model skin file. */
    bool HasModel(const DNItem& item) const;

    /** @brief Checks if an item is a charm, random box, or linked to a charm drop pool. */
    bool HasCharm(const DNItem& item) const noexcept;

    // --- Charm & Gacha Box Probability Management ---

    /** @brief Resolves the linked CharmNum for an item (from type parameters or key table). */
    int32_t GetItemCharmNum(const DNItem& item) const noexcept;

    /** @brief Retrieves the drop reward group for a given CharmNum. */
    const CharmGroup* GetCharmGroup(int32_t charmNum) const;

    /** @brief Returns all sorted unique CharmNums indexed in the active workspace. */
    const std::vector<int32_t>& GetAllCharmNums() const noexcept { return m_allCharmNums; }

    /** @brief Retrieves key unlock requirements for a charm item, or nullptr if none. */
    const CharmKeyInfo* GetCharmKeyInfo(int32_t charmItemId) const;

    /** @brief Updates the probability weight of a specific charm reward row. */
    bool SetCharmItemProb(int32_t rowId, int32_t newWeight);

    /** @brief Normalizes all drop weights in a charm set to a specified total target weight. */
    bool NormalizeCharmGroupWeights(int32_t charmNum, int64_t targetTotalWeight);

    /** @brief Returns total count of indexed charm groups. */
    size_t GetCharmGroupCount() const noexcept { return m_charmGroups.size(); }

    /** @brief Returns total count of charm drop reward entries across all loaded tables. */
    size_t GetTotalCharmRewardCount() const noexcept { return m_totalCharmRewards; }

    // --- Dragon Jewel, Socketing, Set Item & Enchantment APIs ---

    /** @brief Checks if an item is a Dragon Jewel / Jade item. */
    bool IsDragonJewel(const DNItem& item) const noexcept;

    /** @brief Checks if an item has Dragon Jewel sockets. */
    bool HasJewelSockets(const DNItem& item) const noexcept;

    /** @brief Checks if an item is part of an equipment set. */
    bool IsSetItem(const DNItem& item) const noexcept;

    /** @brief Checks if an item has an enhancement upgrade progression curve. */
    bool IsEnhanceable(const DNItem& item) const noexcept;

    /** @brief Retrieves Dragon Jewel metadata for an item ID, or nullptr if none. */
    const DragonJewelInfo* GetDragonJewelInfo(int32_t itemId) const;

    /** @brief Retrieves Dragon Jewel socket configuration by _dragonjeweltype ID. */
    const DragonJewelSlotConfig* GetItemJewelSlots(int32_t dragonJewelType) const;

    /** @brief Retrieves Set Item group definition by _SetItemID. */
    const SetItemGroup* GetSetItemGroup(int32_t setItemId) const;

    /** @brief Returns list of all unique Set Item IDs indexed in the workspace. */
    const std::vector<int32_t>& GetAllSetItemIds() const noexcept { return m_allSetItemIds; }

    /** @brief Searches compatible Dragon Jewels matching slot type and constraints. */
    std::vector<DragonJewelInfo> GetCompatibleJewels(int32_t slotType, int32_t levelLimit = 999, int32_t rank = -1) const;

    /** @brief Finds all equipment items that can socket a given Dragon Jewel. */
    std::vector<int32_t> GetCompatibleEquipmentForJewel(const DragonJewelInfo& jewel) const;

    /** @brief Builds and retrieves the +1..+20 enhancement curve for an Enchant ID. */
    EnchantCurve GetEnchantCurve(int32_t enchantId) const;

    /** @brief Builds and retrieves all potential variants for a Potential ID. */
    PotentialGroup GetPotentialGroup(int32_t potentialId) const;

    /** @brief Updates the Dragon Jewel socket configuration ID on an item. */
    bool SetItemDragonJewelType(int32_t itemId, int32_t newType);

    /** @brief Updates the Enhancement curve ID on an item. */
    bool SetItemEnchantId(int32_t itemId, int32_t newEnchantId);

    /** @brief Modifies Dragon Jewel attributes in `dragonjeweltable.dnt`. */
    bool SetDragonJewelAttributes(int32_t itemId, int32_t type, int32_t elementalType, int32_t applicableSlots);

    /** @brief Returns total count of indexed dragon jewels. */
    size_t GetDragonJewelCount() const noexcept { return m_dragonJewels.size(); }

    /** @brief Returns total count of indexed set items. */
    size_t GetSetItemCount() const noexcept { return m_setItems.size(); }

    /** @brief Returns total count of indexed enchantment curves. */
    size_t GetEnchantCurveCount() const noexcept { return m_enchantIndex.size(); }

private:
    struct ItemCols {
        int nameId = -1, descId = -1, nameParam = -1, descParam = -1, type = -1, rank = -1, levelLimit = -1,
            heroLevelLimit = -1, jobClass = -1, overlap = -1, sell = -1, icon = -1, isCash = -1, ableStorage = -1,
            ableWStorage = -1, gearScore = -1, typeParam1 = -1, typeParam2 = -1, typeParam3 = -1;
        int dragonJewelType = -1;
        int enchantId = -1;
        std::array<int, 10> potential{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        std::array<int, kStateSlotCount> state{}, prob{}, minV{}, maxV{};
    };

    struct WeaponCols {
        int equipType = -1;
        int durability = -1;
        int setItemId = -1;
        int setItemTextId = -1;
        std::array<int, 3> skinNames{ -1, -1, -1 };
    };

    struct PartsCols {
        int parts = -1;
        int durability = -1;
        int skinId = -1;
        int setItemId = -1;
        int setItemTextId = -1;
        std::array<int, 10> playerSkins{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    };

    static ItemCols BindItemCols(const DNTTable& t);
    static WeaponCols BindWeaponCols(const DNTTable& t);
    static PartsCols BindPartsCols(const DNTTable& t);

    int FindMatchingDetailTableIndex(size_t itemTableIdx, bool weapon) const;
    DNTTable* FindMutableDetailTableForItem(const DNItem& item, size_t* outIdx = nullptr);

    void ResolveItemText(DNItem& item) const;
    void ReadItemRowDirect(const DNTRow& row, uint16_t tableIdx, DNItem& out) const;
    void ReadItemRow(int32_t id, uint16_t tableIdx, DNItem& out) const;
    void WriteItemRow(const DNItem& item);
    bool IsIdUsedAnywhere(int32_t id) const;
    void IndexModels() const;

    std::filesystem::path m_resourcePath;
    UIStringManager m_uiStrings;
    double m_lastLoadMs = 0.0;

    std::vector<DNTTable> m_itemTables;
    std::vector<ItemCols> m_itemCols;
    std::vector<bool> m_dirtyItems;

    std::vector<DNTTable> m_weaponTables;
    std::vector<WeaponCols> m_weaponCols;
    std::vector<bool> m_dirtyWeapons;
    std::unordered_map<int32_t, uint16_t> m_weaponTableById;

    std::vector<DNTTable> m_partsTables;
    std::vector<PartsCols> m_partsCols;
    std::vector<bool> m_dirtyParts;
    std::unordered_map<int32_t, uint16_t> m_partsTableById;

    DNTTable m_fileTable;
    int m_fileNameCol = -1;

    DNTTable m_jobTable;
    int m_jobNameCol = -1;

    struct CharmTablePair {
        std::filesystem::path itemPath;
        std::filesystem::path probPath;
        DNTTable itemTable;
        DNTTable probTable;
        int cItemId = -1;
        int cCount = -1;
        int cCharmNum = -1;
        int cMsg = -1;
        int cGold = -1;
        int cPeriod = -1;
        int cMsgType = -1;
        int cItemNameId = -1;
        int cProbInItem = -1;
        int cProbInProb = -1;
        int cPotenProb = -1;
        bool dirtyItem = false;
        bool dirtyProb = false;
    };

    std::vector<CharmTablePair> m_charmTables;
    DNTTable m_charmKeyTable;
    std::unordered_map<int32_t, CharmKeyInfo> m_charmKeys;
    std::unordered_map<int32_t, int32_t> m_charmItemToCharmNum;
    std::unordered_map<int32_t, CharmGroup> m_charmGroups;
    std::vector<int32_t> m_allCharmNums;
    std::unordered_map<int32_t, std::pair<size_t, int32_t>> m_charmRowLookup;
    size_t m_totalCharmRewards = 0;

    bool m_dirtyStrings = false;

    DNTTable m_dragonJewelTable;
    DNTTable m_dragonJewelSlotTable;
    std::vector<DNTTable> m_setItemTables;
    std::vector<DNTTable> m_enchantTables;
    std::vector<DNTTable> m_potentialTables;

    std::unordered_map<int32_t, DragonJewelInfo> m_dragonJewels;
    std::unordered_map<int32_t, DragonJewelSlotConfig> m_jewelSlotConfigs;
    std::unordered_map<int32_t, SetItemGroup> m_setItems;
    std::vector<int32_t> m_allSetItemIds;
    std::unordered_map<int32_t, std::vector<std::pair<uint16_t, uint32_t>>> m_enchantIndex;
    std::unordered_map<int32_t, std::vector<std::pair<uint16_t, uint32_t>>> m_potentialIndex;
    bool m_dirtyDragonJewelTable = false;

    void AdjustMidRef(uint32_t mid, int delta) noexcept;

    std::deque<DNItem> m_items;
    std::unordered_map<int32_t, size_t> m_indexById;
    std::unordered_map<int32_t, int32_t> m_skinCombine;
    std::unordered_map<uint32_t, uint32_t> m_midRefCount;
    std::unordered_set<int32_t> m_modifiedItemIds;

    mutable std::unordered_map<std::string, std::filesystem::path> m_modelIndex;
    mutable std::atomic<bool> m_modelIndexBuilt{ false };
    mutable std::mutex m_modelMutex;
};

} // namespace DNItemMaker::Data
