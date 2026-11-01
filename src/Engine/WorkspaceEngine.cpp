#include "WorkspaceEngine.h"

#include "Common/Logs/Logs.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <format>

namespace DNItemMaker::Engine {

namespace {

constexpr int kIconTileSize = 50;
constexpr int kIconsPerPage = 200;
constexpr size_t kMaxUndoDepth = 32;
constexpr size_t kMaxUndoBatchItems = 5000;

constexpr char AsciiLower(char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    return static_cast<char>((u >= 'A' && u <= 'Z') ? (u + ('a' - 'A')) : u);
}

bool IsValidResourceFolder(const std::filesystem::path& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(path / "uistring" / "uistring.xml", ec) &&
           std::filesystem::is_directory(path / "ext", ec);
}

std::string NormalizePathKey(const std::filesystem::path& p) {
    std::string s = p.string();
    for (char& ch : s) {
        ch = (ch == '\\') ? '/' : AsciiLower(ch);
    }
    return s;
}

struct BaseClassSpec {
    int32_t jobId;
    const char* name;
    bool primarilyMagical;
};

constexpr BaseClassSpec kBaseClasses[10] = {
    { 1,  "Warrior",   false },
    { 2,  "Archer",    false },
    { 3,  "Sorceress", true  },
    { 4,  "Cleric",    true  },
    { 5,  "Academic",  true  },
    { 6,  "Kali",      true  },
    { 7,  "Assassin",  false },
    { 8,  "Lancea",    false },
    { 9,  "Machina",   false },
    { 201, "Vandar",   false },
};

void AdaptStatsForDamageType(Data::DNItem& item, bool targetMagical) noexcept {
    for (auto& st : item.states) {
        if (st.type < 0) continue;
        if (targetMagical) {
            switch (st.type) {
                case 0:  st.type = 2;  break; // STR -> INT
                case 1:  st.type = 2;  break; // AGI -> INT
                case 4:  st.type = 6;  break; // Phys Atk Min -> Mag Atk Min
                case 5:  st.type = 7;  break; // Phys Atk Max -> Mag Atk Max
                case 32: st.type = 33; break; // Phys Atk (Min+Max) -> Mag Atk (Min+Max)
                case 50: st.type = 52; break; // STR % -> INT %
                case 51: st.type = 52; break; // AGI % -> INT %
                case 54: st.type = 56; break; // Phys Atk Min % -> Mag Atk Min %
                case 55: st.type = 57; break; // Phys Atk Max % -> Mag Atk Max %
                default: break;
            }
        } else {
            switch (st.type) {
                case 2:  st.type = 0;  break; // INT -> STR
                case 6:  st.type = 4;  break; // Mag Atk Min -> Phys Atk Min
                case 7:  st.type = 5;  break; // Mag Atk Max -> Phys Atk Max
                case 33: st.type = 32; break; // Mag Atk (Min+Max) -> Phys Atk (Min+Max)
                case 52: st.type = 50; break; // INT % -> STR %
                case 56: st.type = 54; break; // Mag Atk Min % -> Phys Atk Min %
                case 57: st.type = 55; break; // Mag Atk Max % -> Phys Atk Max %
                default: break;
            }
        }
    }
}

} // namespace

WorkspaceEngine::WorkspaceEngine() = default;

WorkspaceEngine::~WorkspaceEngine() {
    if (m_indexWorker.joinable()) {
        m_indexWorker.join();
    }
}

void WorkspaceEngine::InitializeServices(const std::filesystem::path& appDirectory) {
    const auto dbPath = appDirectory / "DNItemMaker_Recovery.db";
    m_recoveryDb.Initialize(dbPath);
}

std::filesystem::path WorkspaceEngine::DetectInitialWorkspacePath() const {
    const std::string saved = m_recoveryDb.GetSetting("last_resource_path");
    if (!saved.empty()) {
        const std::filesystem::path savedPath(saved);
        if (IsValidResourceFolder(savedPath)) return savedPath;
    }

    const std::filesystem::path candidates[] = {
        R"(D:\GameRes\resource)",
        std::filesystem::current_path() / "resource",
        std::filesystem::current_path().parent_path() / "resource",
    };
    for (const auto& c : candidates) {
        if (IsValidResourceFolder(c)) return c;
    }
    return {};
}

bool WorkspaceEngine::OpenWorkspace(const std::filesystem::path& resourcePath,
                                    size_t* outRecoveredDrafts,
                                    std::function<void()> onModelIndexReady,
                                    Data::LoadProgressCallback onProgress) {
    if (outRecoveredDrafts) *outRecoveredDrafts = 0;

    if (m_indexWorker.joinable()) {
        m_indexWorker.join();
    }

    m_undoStack.clear();

    {
        std::scoped_lock lock(m_assetMutex);
        m_iconTileCache.clear();
        m_atlasCache.clear();
        m_textureCache.clear();
        m_skinCache.clear();
        m_meshCache.clear();
        m_resourceCacheTag.clear();
        m_maxIconPage = 1;
    }

    if (onProgress) {
        onProgress(0.08f, "Opening Workspace...", resourcePath.filename().string());
    }
    if (!m_items.LoadWorkspace(resourcePath, onProgress)) {
        return false;
    }

    m_resourceCacheTag = m_items.GetResourcePath().filename().string();
    m_recoveryDb.SetSetting("last_resource_path", resourcePath.string());
    ScanIconPageCount();

    // Replay any unsaved SQLite WAL drafts on top of the loaded tables.
    const auto drafts = m_recoveryDb.LoadDrafts([this](int32_t id) { return m_items.GetItem(id); });
    size_t replayed = 0;
    for (const auto& draft : drafts) {
        if (m_items.ApplyItem(draft)) {
            ++replayed;
        }
    }
    if (replayed > 0) {
        LOG_INFO(Channels::sqlite, "Replayed {} unsaved item draft(s) from SQLite recovery journal.", replayed);
    }
    if (outRecoveredDrafts) *outRecoveredDrafts = replayed;

    if (onProgress) {
        onProgress(0.98f, "Synchronizing SQLite Recovery...",
                   replayed > 0 ? std::format("Replayed {} draft(s)", replayed) : "All tables clean");
    }

    // Kick off background indexing of weapon/char/item 3D files so UI opens immediately.
    m_indexWorker = std::jthread([this, cb = std::move(onModelIndexReady)]() {
        m_items.PrepareModelIndex();
        if (cb) {
            cb();
        }
    });

    return true;
}

void WorkspaceEngine::ScanIconPageCount() {
    int maxPage = 1;
    std::error_code ec;
    const auto mainbarDir = m_items.GetResourcePath() / "ui" / "mainbar";
    if (std::filesystem::is_directory(mainbarDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(mainbarDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string name = entry.path().filename().string();
            for (char& ch : name) ch = AsciiLower(ch);
            if (name.starts_with("itemicon") && name.ends_with(".dds") && name.size() > 12) {
                int page = 0;
                const char* first = name.data() + 8;
                const char* last = name.data() + (name.size() - 4);
                const auto [ptr, parseEc] = std::from_chars(first, last, page);
                if (parseEc == std::errc{} && ptr == last && page > maxPage) {
                    maxPage = page;
                }
            }
        }
    }
    std::scoped_lock lock(m_assetMutex);
    m_maxIconPage = maxPage;
}

void WorkspaceEngine::PushUndoEntry(UndoEntry entry) {
    if (entry.records.empty()) return;
    while (m_undoStack.size() >= kMaxUndoDepth) {
        m_undoStack.pop_front();
    }
    m_undoStack.push_back(std::move(entry));
}

WorkspaceEngine::ItemUndoRecord WorkspaceEngine::CaptureExistingSnapshot(int32_t id) const {
    ItemUndoRecord rec;
    rec.id = id;
    if (const auto* existing = m_items.GetItem(id)) {
        rec.existedBefore = true;
        rec.wasModifiedBefore = m_items.IsItemModified(id);
        rec.snapshot = *existing;
    } else {
        rec.existedBefore = false;
    }
    return rec;
}

std::string WorkspaceEngine::GetUndoLabel() const {
    return m_undoStack.empty() ? std::string{} : m_undoStack.back().label;
}

std::vector<std::string> WorkspaceEngine::GetUndoHistoryLabels() const {
    std::vector<std::string> out;
    out.reserve(m_undoStack.size());
    for (auto it = m_undoStack.rbegin(); it != m_undoStack.rend(); ++it) {
        out.push_back(std::format("{} ({} item{})",
                                  it->label,
                                  it->records.size(),
                                  it->records.size() == 1 ? "" : "s"));
    }
    return out;
}

bool WorkspaceEngine::UndoLastAction(std::string& outSummary) {
    if (m_undoStack.empty()) {
        outSummary = "Nothing to undo.";
        return false;
    }

    UndoEntry entry = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    size_t restored = 0;
    size_t deleted = 0;
    std::vector<const Data::DNItem*> toSaveDrafts;

    for (auto it = entry.records.rbegin(); it != entry.records.rend(); ++it) {
        const ItemUndoRecord& rec = *it;
        if (!rec.existedBefore) {
            if (m_items.DeleteItem(rec.id)) {
                m_recoveryDb.RemoveDraft(rec.id);
                ++deleted;
            }
        } else {
            if (m_items.ApplyItem(rec.snapshot)) {
                m_items.SetItemModifiedFlag(rec.id, rec.wasModifiedBefore);
                if (rec.wasModifiedBefore) {
                    if (const auto* cur = m_items.GetItem(rec.id)) {
                        toSaveDrafts.push_back(cur);
                    }
                } else {
                    m_recoveryDb.RemoveDraft(rec.id);
                }
                ++restored;
            }
        }
    }

    if (!toSaveDrafts.empty()) {
        m_recoveryDb.SaveDraftsBatch(toSaveDrafts);
    }

    outSummary = std::format("Undid '{}': {} restored, {} newly-created removed.",
                             entry.label, restored, deleted);
    LOG_INFO(Channels::engine, "{}", outSummary);
    return (restored + deleted) > 0;
}

bool WorkspaceEngine::ApplyItemEdit(const Data::DNItem& edited) {
    ItemUndoRecord before = CaptureExistingSnapshot(edited.id);
    if (!before.existedBefore) return false;

    if (!m_items.ApplyItem(edited)) return false;
    if (const auto* updated = m_items.GetItem(edited.id)) {
        m_recoveryDb.SaveDraft(*updated);
    }

    UndoEntry u;
    u.label = std::format("Edit #{} '{}'", edited.id, edited.name.empty() ? edited.nameTemplate : edited.name);
    u.records.push_back(std::move(before));
    PushUndoEntry(std::move(u));
    return true;
}

int32_t WorkspaceEngine::DuplicateItem(int32_t sourceId, const std::string& newName, int targetTableIndex, int32_t requestedNewId) {
    const int32_t newId = m_items.DuplicateItem(sourceId, newName, targetTableIndex, requestedNewId);
    if (newId > 0) {
        if (const auto* created = m_items.GetItem(newId)) {
            m_recoveryDb.SaveDraft(*created);
            UndoEntry u;
            u.label = std::format("Duplicate #{} -> #{}", sourceId, newId);
            u.records.push_back(ItemUndoRecord{ newId, false, false, {} });
            PushUndoEntry(std::move(u));
        }
    }
    return newId;
}

int32_t WorkspaceEngine::CreateNewItem(int targetTableIndex, int32_t rawType, int32_t equipSubType,
                                       const std::string& newName, int32_t requestedNewId) {
    const int32_t newId = m_items.CreateNewItem(targetTableIndex, rawType, equipSubType, newName, requestedNewId);
    if (newId > 0) {
        if (const auto* created = m_items.GetItem(newId)) {
            m_recoveryDb.SaveDraft(*created);
            UndoEntry u;
            u.label = std::format("Create #{} '{}'", newId, created->name);
            u.records.push_back(ItemUndoRecord{ newId, false, false, {} });
            PushUndoEntry(std::move(u));
        }
    }
    return newId;
}

bool WorkspaceEngine::DeleteItem(int32_t id) {
    if (!m_items.DeleteItem(id)) return false;
    m_recoveryDb.RemoveDraft(id);
    return true;
}

bool WorkspaceEngine::SetRawItemCell(int32_t itemId, bool detailTable, size_t colIdx, std::string_view textValue) {
    ItemUndoRecord before = CaptureExistingSnapshot(itemId);
    if (!m_items.SetRawItemCell(itemId, detailTable, colIdx, textValue)) return false;
    if (const auto* updated = m_items.GetItem(itemId)) {
        m_recoveryDb.SaveDraft(*updated);
    }
    if (before.existedBefore && !detailTable) {
        UndoEntry u;
        u.label = std::format("Raw cell edit on #{}", itemId);
        u.records.push_back(std::move(before));
        PushUndoEntry(std::move(u));
    }
    return true;
}

bool WorkspaceEngine::ForkExclusiveMids(int32_t itemId, bool forkName, bool forkDesc) {
    const auto* cur = m_items.GetItem(itemId);
    if (!cur) return false;

    ItemUndoRecord before = CaptureExistingSnapshot(itemId);
    Data::DNItem copy = *cur;
    bool changed = false;

    if (forkName) {
        copy.nameTemplate = copy.name;
        copy.nameParam.clear();
        copy.nameId = m_items.AllocateUIStringMid(copy.nameTemplate);
        changed = true;
    }
    if (forkDesc) {
        copy.descTemplate = copy.description;
        copy.descParam.clear();
        copy.descId = m_items.AllocateUIStringMid(copy.descTemplate);
        changed = true;
    }
    if (!changed || !m_items.ApplyItem(copy)) return false;

    if (const auto* updated = m_items.GetItem(itemId)) {
        m_recoveryDb.SaveDraft(*updated);
    }
    UndoEntry u;
    u.label = std::format("Fork exclusive MIDs on #{}", itemId);
    u.records.push_back(std::move(before));
    PushUndoEntry(std::move(u));
    return true;
}

bool WorkspaceEngine::CopyVisualFromItem(int32_t targetId, int32_t sourceId, bool copyIcon, bool copy3DSkin) {
    ItemUndoRecord before = CaptureExistingSnapshot(targetId);
    if (!before.existedBefore) return false;
    if (!m_items.CopyVisualFromItem(targetId, sourceId, copyIcon, copy3DSkin)) return false;

    if (const auto* updated = m_items.GetItem(targetId)) {
        m_recoveryDb.SaveDraft(*updated);
    }
    if (copyIcon && !copy3DSkin) {
        UndoEntry u;
        u.label = std::format("Copy icon #{} -> #{}", sourceId, targetId);
        u.records.push_back(std::move(before));
        PushUndoEntry(std::move(u));
    }
    return true;
}

bool WorkspaceEngine::CloneDetailRowFromItem(int32_t targetId, int32_t sourceId) {
    if (!m_items.CloneDetailRowFromItem(targetId, sourceId)) return false;
    if (const auto* updated = m_items.GetItem(targetId)) {
        m_recoveryDb.SaveDraft(*updated);
    }
    return true;
}

std::vector<int32_t> WorkspaceEngine::GenerateRankTierSet(const TierSetConfig& cfg, std::string& outSummary) {
    std::vector<int32_t> createdIds;
    const Data::DNItem* src = m_items.GetItem(cfg.sourceItemId);
    if (!src) {
        outSummary = "Select a valid template item first.";
        return createdIds;
    }

    const std::string cleanBase = cfg.baseName.empty() ? src->name : cfg.baseName;
    const double stepRatio = 1.0 + std::clamp(cfg.statGrowthPerRankPct, -80.0, 500.0) / 100.0;

    UndoEntry undo;
    undo.label = std::format("Generate Tier Set from #{}", src->id);
    std::vector<const Data::DNItem*> drafts;

    for (int32_t r = 0; r <= 6; ++r) {
        if (!cfg.includeRanks[static_cast<size_t>(r)]) continue;

        const std::string tierName = std::format("[{}] {}", Data::ItemManager::RankName(r), cleanBase);
        const int32_t newId = m_items.DuplicateItem(src->id, tierName, cfg.targetTableIndex, 0);
        if (newId <= 0) continue;

        const Data::DNItem* createdPtr = m_items.GetItem(newId);
        if (!createdPtr) continue;

        Data::DNItem tierItem = *createdPtr;
        tierItem.rank = r;

        if (cfg.useAutoCurve) {
            Data::ItemManager::AutoScaleStatsByLevelAndRank(tierItem);
        } else {
            const int rankDelta = r - src->rank;
            const double mult = std::pow(stepRatio, static_cast<double>(rankDelta));
            Data::ItemManager::ScaleItemStats(tierItem, mult, false);
        }

        if (cfg.autoRecalcEconomy) {
            tierItem.gearScore = Data::ItemManager::ComputeSuggestedGearScore(tierItem);
            tierItem.sellAmount = Data::ItemManager::ComputeSuggestedSellAmount(tierItem);
            tierItem.durability = Data::ItemManager::ComputeSuggestedDurability(tierItem);
        }
        if (cfg.autoRichTooltip) {
            tierItem.descTemplate = m_items.GenerateRichTooltipDescription(tierItem);
            tierItem.descParam.clear();
        }

        m_items.ApplyItem(tierItem);
        if (const auto* finalPtr = m_items.GetItem(newId)) {
            drafts.push_back(finalPtr);
        }
        undo.records.push_back(ItemUndoRecord{ newId, false, false, {} });
        createdIds.push_back(newId);
    }

    if (!drafts.empty()) {
        m_recoveryDb.SaveDraftsBatch(drafts);
    }
    PushUndoEntry(std::move(undo));

    outSummary = std::format("Generated {} rank-tier item(s) for '{}'.", createdIds.size(), cleanBase);
    return createdIds;
}

std::vector<int32_t> WorkspaceEngine::GenerateClassEquipmentSeries(const ClassSeriesConfig& cfg, std::string& outSummary) {
    std::vector<int32_t> createdIds;
    const Data::DNItem* src = m_items.GetItem(cfg.sourceItemId);
    if (!src) {
        outSummary = "Select a valid template item first.";
        return createdIds;
    }

    const std::string tmpl = cfg.seriesNameTemplate.empty()
                                 ? std::format("{{CLASS}} {}", src->name)
                                 : cfg.seriesNameTemplate;

    UndoEntry undo;
    undo.label = std::format("Generate Class Series from #{}", src->id);
    std::vector<const Data::DNItem*> drafts;

    for (size_t c = 0; c < 10; ++c) {
        if (!cfg.includeClasses[c]) continue;
        const auto& cls = kBaseClasses[c];

        std::string itemName = tmpl;
        const size_t tokPos = itemName.find("{CLASS}");
        if (tokPos != std::string::npos) {
            itemName.replace(tokPos, 7, cls.name);
        } else {
            itemName = std::format("{} {}", cls.name, itemName);
        }

        const int32_t newId = m_items.DuplicateItem(src->id, itemName, cfg.targetTableIndex, 0);
        if (newId <= 0) continue;

        const Data::DNItem* createdPtr = m_items.GetItem(newId);
        if (!createdPtr) continue;

        Data::DNItem clsItem = *createdPtr;
        clsItem.jobClass = std::to_string(cls.jobId);

        if (cfg.adaptPhysicalMagicalStats) {
            AdaptStatsForDamageType(clsItem, cls.primarilyMagical);
        }
        if (cfg.autoRichTooltip) {
            clsItem.descTemplate = m_items.GenerateRichTooltipDescription(clsItem);
            clsItem.descParam.clear();
        }

        m_items.ApplyItem(clsItem);
        if (const auto* finalPtr = m_items.GetItem(newId)) {
            drafts.push_back(finalPtr);
        }
        undo.records.push_back(ItemUndoRecord{ newId, false, false, {} });
        createdIds.push_back(newId);
    }

    if (!drafts.empty()) {
        m_recoveryDb.SaveDraftsBatch(drafts);
    }
    PushUndoEntry(std::move(undo));

    outSummary = std::format("Generated {} class-specific item(s) across selected Dragon Nest classes.", createdIds.size());
    return createdIds;
}

size_t WorkspaceEngine::ExecuteBatchEdit(const std::vector<int32_t>& itemIds, const BatchEditSpec& spec, std::string& outSummary) {
    if (itemIds.empty()) {
        outSummary = "No items in target set.";
        return 0;
    }

    const bool hasRename = !spec.namePrefix.empty() || !spec.nameSuffix.empty();
    const bool anyOp = spec.scaleStats || spec.autoCurveStats || spec.sanitizeStats ||
                       spec.setRank || spec.setLevelLimit || spec.setHeroLevelLimit ||
                       spec.setOverlapCount || spec.setCashFlag || spec.setStorageFlags ||
                       spec.autoRecalcEconomy || hasRename;
    if (!anyOp) {
        outSummary = "Select at least one batch transformation.";
        return 0;
    }

    UndoEntry undo;
    undo.label = std::format("Batch Edit ({} items)", itemIds.size());
    const bool recordUndo = itemIds.size() <= kMaxUndoBatchItems;
    if (recordUndo) {
        undo.records.reserve(itemIds.size());
    }

    std::vector<const Data::DNItem*> drafts;
    drafts.reserve(itemIds.size());
    size_t modifiedCount = 0;

    for (int32_t id : itemIds) {
        const Data::DNItem* cur = m_items.GetItem(id);
        if (!cur) continue;

        if (recordUndo) {
            undo.records.push_back(CaptureExistingSnapshot(id));
        }

        Data::DNItem edited = *cur;

        if (spec.setRank) edited.rank = std::clamp(spec.newRank, 0, 6);
        if (spec.setLevelLimit) edited.levelLimit = std::clamp(spec.newLevelLimit, 1, 999);
        if (spec.setHeroLevelLimit) edited.heroLevelLimit = std::max(0, spec.newHeroLevelLimit);
        if (spec.setOverlapCount) edited.overlapCount = std::max(1, spec.newOverlapCount);
        if (spec.setCashFlag) edited.isCash = spec.cashValue;
        if (spec.setStorageFlags) {
            edited.ableStorage = spec.ableStorage;
            edited.ableWStorage = spec.ableWStorage;
        }

        if (spec.autoCurveStats) {
            Data::ItemManager::AutoScaleStatsByLevelAndRank(edited);
        }
        if (spec.scaleStats && std::fabs(spec.statMultiplier - 1.0) > 1e-6) {
            Data::ItemManager::ScaleItemStats(edited, spec.statMultiplier, false);
        }
        if (spec.sanitizeStats) {
            Data::ItemManager::CompactAndSanitizeStats(edited);
        }
        if (spec.autoRecalcEconomy) {
            edited.gearScore = Data::ItemManager::ComputeSuggestedGearScore(edited);
            edited.sellAmount = Data::ItemManager::ComputeSuggestedSellAmount(edited);
            edited.durability = Data::ItemManager::ComputeSuggestedDurability(edited);
        }

        if (hasRename) {
            const std::string newResolvedName = std::format("{}{}{}", spec.namePrefix, edited.name, spec.nameSuffix);
            if (spec.autoForkSharedMidsOnRename && (edited.nameId == 0 || m_items.CountItemsUsingMid(edited.nameId) > 1 || !edited.nameParam.empty())) {
                edited.nameId = m_items.AllocateUIStringMid(newResolvedName);
            }
            edited.nameParam.clear();
            edited.nameTemplate = newResolvedName;
        }

        if (m_items.ApplyItem(edited)) {
            ++modifiedCount;
            if (const auto* updated = m_items.GetItem(id)) {
                drafts.push_back(updated);
            }
        }
    }

    if (!drafts.empty()) {
        m_recoveryDb.SaveDraftsBatch(drafts);
    }
    if (recordUndo && !undo.records.empty()) {
        PushUndoEntry(std::move(undo));
    }

    outSummary = std::format("Batch updated {} item(s){}.",
                             modifiedCount,
                             recordUndo ? " (Undo available)" : " (Large batch >5000 items: undo snapshot skipped)");
    return modifiedCount;
}

size_t WorkspaceEngine::ImportItemsJson(std::string_view jsonText, int targetTableIndex,
                                        std::vector<int32_t>& outCreatedIds, std::string& outSummary) {
    const size_t count = m_items.ImportItemsFromJson(jsonText, targetTableIndex, outCreatedIds, outSummary);
    if (count > 0) {
        UndoEntry undo;
        undo.label = std::format("Import JSON ({} items)", count);
        std::vector<const Data::DNItem*> drafts;
        drafts.reserve(outCreatedIds.size());
        for (int32_t id : outCreatedIds) {
            undo.records.push_back(ItemUndoRecord{ id, false, false, {} });
            if (const auto* ptr = m_items.GetItem(id)) {
                drafts.push_back(ptr);
            }
        }
        if (!drafts.empty()) {
            m_recoveryDb.SaveDraftsBatch(drafts);
        }
        PushUndoEntry(std::move(undo));
    }
    return count;
}

bool WorkspaceEngine::CommitAllToDisk(std::string& report) {
    const bool ok = m_items.CommitToDisk(report);
    if (ok) {
        m_recoveryDb.Clear();
        m_undoStack.clear();
    }
    return ok;
}

bool WorkspaceEngine::SaveModifiedToFolder(const std::filesystem::path& outputFolder, std::string& report) {
    const bool ok = m_items.SaveModifiedToFolder(outputFolder, report);
    if (ok) {
        m_recoveryDb.Clear();
        m_undoStack.clear();
    }
    return ok;
}

void WorkspaceEngine::DiscardRecoveredDrafts() {
    m_recoveryDb.Clear();
    m_undoStack.clear();
}

std::shared_ptr<const Graphics::ImageBuffer> WorkspaceEngine::GetIconAtlasPage(int pageNumber) {
    if (pageNumber < 1 || m_items.GetResourcePath().empty()) return nullptr;

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_atlasCache.find(pageNumber);
        if (it != m_atlasCache.end()) return it->second;
    }

    const auto atlasPath = m_items.GetResourcePath() / "ui" / "mainbar" / std::format("itemicon{:02d}.dds", pageNumber);
    std::error_code ec;
    if (!std::filesystem::exists(atlasPath, ec)) return nullptr;

    auto decoded = std::make_shared<Graphics::ImageBuffer>();
    if (!Graphics::DDSParser::DecodeFile(atlasPath, *decoded)) return nullptr;

    {
        std::scoped_lock lock(m_assetMutex);
        if (m_atlasCache.size() >= 24) {
            m_atlasCache.clear();
        }
        m_atlasCache[pageNumber] = decoded;
    }
    return decoded;
}

void WorkspaceEngine::ClearNativeCaches() {
    std::scoped_lock lock(m_assetMutex);
    m_iconTileCache.clear();
    m_atlasCache.clear();
    m_textureCache.clear();
    m_skinCache.clear();
    m_meshCache.clear();
}

bool WorkspaceEngine::GetItemIcon(int32_t iconIndex, Graphics::ImageBuffer& outTile) {
    if (iconIndex < 0 || m_items.GetResourcePath().empty()) return false;

    {
        std::scoped_lock lock(m_assetMutex);
        auto it = m_iconTileCache.find(iconIndex);
        if (it != m_iconTileCache.end()) {
            outTile = it->second;
            return true;
        }
    }

    if (!m_items.GetResourcePath().empty()) {
        const auto customPath = m_items.GetResourcePath() / "ui" / "mainbar" / "custom" / std::format("icon_{:05d}.dds", iconIndex);
        std::error_code ec;
        if (std::filesystem::exists(customPath, ec)) {
            Graphics::ImageBuffer customImg;
            if (Graphics::DDSParser::DecodeFile(customPath, customImg)) {
                std::scoped_lock lock(m_assetMutex);
                m_iconTileCache[iconIndex] = customImg;
                outTile = customImg;
                return true;
            }
        }
    }

    const int page = iconIndex / kIconsPerPage + 1;
    const int tile = iconIndex % kIconsPerPage;
    const auto atlas = GetIconAtlasPage(page);
    if (!atlas) return false;

    if (!Graphics::DDSParser::CropIconTile(*atlas, tile, kIconTileSize, kIconTileSize, outTile)) {
        return false;
    }

    {
        std::scoped_lock lock(m_assetMutex);
        if (m_iconTileCache.size() >= 4096) {
            auto it = m_iconTileCache.begin();
            std::advance(it, m_iconTileCache.size() / 2);
            m_iconTileCache.erase(m_iconTileCache.begin(), it);
        }
        m_iconTileCache[iconIndex] = outTile;
    }
    return true;
}

std::shared_ptr<const Graphics::ImageBuffer> WorkspaceEngine::LoadTextureCached(const std::filesystem::path& ddsPath) {
    if (ddsPath.empty()) return nullptr;
    const std::string key = NormalizePathKey(ddsPath);

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_textureCache.find(key);
        if (it != m_textureCache.end()) return it->second;
    }

    auto img = std::make_shared<Graphics::ImageBuffer>();
    if (!Graphics::DDSParser::DecodeFile(ddsPath, *img)) return nullptr;

    {
        std::scoped_lock lock(m_assetMutex);
        if (m_textureCache.size() >= 256) {
            auto it = m_textureCache.begin();
            std::advance(it, m_textureCache.size() / 2);
            m_textureCache.erase(m_textureCache.begin(), it);
        }
        m_textureCache[key] = img;
    }
    return img;
}

std::shared_ptr<const Graphics::ParsedSkin> WorkspaceEngine::LoadSkinCached(const std::filesystem::path& skinPath) {
    if (skinPath.empty()) return nullptr;
    const std::string key = NormalizePathKey(skinPath);

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_skinCache.find(key);
        if (it != m_skinCache.end()) return it->second;
    }

    auto parsed = std::make_shared<Graphics::ParsedSkin>(Graphics::SkinParser::LoadFromFile(skinPath));
    if (parsed->valid) {
        std::scoped_lock lock(m_assetMutex);
        if (m_skinCache.size() >= 256) {
            auto it = m_skinCache.begin();
            std::advance(it, m_skinCache.size() / 2);
            m_skinCache.erase(m_skinCache.begin(), it);
        }
        m_skinCache[key] = parsed;
    }
    return parsed;
}

std::shared_ptr<const Graphics::ParsedMesh> WorkspaceEngine::LoadMeshCached(const std::filesystem::path& meshPath) {
    if (meshPath.empty()) return nullptr;
    const std::string key = NormalizePathKey(meshPath);

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_meshCache.find(key);
        if (it != m_meshCache.end()) return it->second;
    }

    auto parsed = std::make_shared<Graphics::ParsedMesh>(Graphics::MeshParser::LoadFromFile(meshPath));
    if (parsed->valid) {
        std::scoped_lock lock(m_assetMutex);
        if (m_meshCache.size() >= 256) {
            auto it = m_meshCache.begin();
            std::advance(it, m_meshCache.size() / 2);
            m_meshCache.erase(m_meshCache.begin(), it);
        }
        m_meshCache[key] = parsed;
    }
    return parsed;
}

ModelAssetBundle WorkspaceEngine::LoadModelForItem(const Data::DNItem& item) {
    ModelAssetBundle bundle;
    if (item.rawType != 0 && item.rawType != 1) {
        bundle.statusSummary = "Non-equipment item (no 3D weapon/armor skin bound)";
        return bundle;
    }

    bundle.skinPath = m_items.FindSkinPath(item);
    if (bundle.skinPath.empty()) {
        bundle.statusSummary = "No .skn skin entry bound in weapon/parts table";
        return bundle;
    }

    bundle.skin = LoadSkinCached(bundle.skinPath);
    if (!bundle.skin || !bundle.skin->valid || bundle.skin->meshName.empty()) {
        bundle.statusSummary = std::format("Invalid skin {}: {}",
                                           bundle.skinPath.filename().string(),
                                           bundle.skin ? bundle.skin->errorMessage : "load failed");
        return bundle;
    }

    bundle.meshPath = m_items.FindModelResource(bundle.skin->meshName, bundle.skinPath);
    if (bundle.meshPath.empty()) {
        bundle.statusSummary = std::format("Skin '{}' references missing mesh '{}'",
                                           bundle.skinPath.filename().string(), bundle.skin->meshName);
        return bundle;
    }

    bundle.mesh = LoadMeshCached(bundle.meshPath);
    if (!bundle.mesh || !bundle.mesh->valid) {
        bundle.statusSummary = std::format("Invalid mesh {}: {}",
                                           bundle.meshPath.filename().string(),
                                           bundle.mesh ? bundle.mesh->errorMessage : "load failed");
        return bundle;
    }

    bundle.materials.resize(bundle.skin->subSkins.size());
    for (size_t i = 0; i < bundle.skin->subSkins.size(); ++i) {
        const auto& sub = bundle.skin->subSkins[i];
        auto& mat = bundle.materials[i];
        mat.emissivePower = sub.emissivePower;
        mat.alphaTest = !sub.alphaBlend;

        if (!sub.diffuseTexture.empty()) {
            const auto texPath = m_items.FindModelResource(sub.diffuseTexture, bundle.skinPath);
            if (!texPath.empty()) {
                mat.diffuse = LoadTextureCached(texPath);
                if (mat.diffuse) {
                    const std::string tn = texPath.filename().string();
                    if (std::find(bundle.textureNames.begin(), bundle.textureNames.end(), tn) == bundle.textureNames.end()) {
                        bundle.textureNames.push_back(tn);
                    }
                }
            }
        }
        if (!sub.emissiveTexture.empty()) {
            const auto emiPath = m_items.FindModelResource(sub.emissiveTexture, bundle.skinPath);
            if (!emiPath.empty()) {
                mat.emissive = LoadTextureCached(emiPath);
            }
        }
    }

    bundle.hasModel = true;
    bundle.statusSummary = std::format("{} -> {} ({} submeshes, {} tris, {} verts)",
                                       bundle.skinPath.filename().string(),
                                       bundle.meshPath.filename().string(),
                                       bundle.mesh->submeshes.size(),
                                       bundle.mesh->GetTotalTriangles(),
                                       bundle.mesh->GetTotalVertices());
    return bundle;
}


std::shared_ptr<const Graphics::ParsedAnimationSet> WorkspaceEngine::LoadAniCached(const std::filesystem::path& aniPath) {
    if (aniPath.empty()) return nullptr;
    const std::string key = NormalizePathKey(aniPath);

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_aniCache.find(key);
        if (it != m_aniCache.end()) return it->second;
    }

    auto parsed = std::make_shared<Graphics::ParsedAnimationSet>(Graphics::AniParser::LoadFromFile(aniPath));
    if (parsed->valid) {
        std::scoped_lock lock(m_assetMutex);
        if (m_aniCache.size() >= 128) {
            auto it = m_aniCache.begin();
            std::advance(it, m_aniCache.size() / 2);
            m_aniCache.erase(m_aniCache.begin(), it);
        }
        m_aniCache[key] = parsed;
    }
    return parsed;
}

std::shared_ptr<const Graphics::ParsedActionFile> WorkspaceEngine::LoadActCached(const std::filesystem::path& actPath) {
    if (actPath.empty()) return nullptr;
    const std::string key = NormalizePathKey(actPath);

    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_actCache.find(key);
        if (it != m_actCache.end()) return it->second;
    }

    auto parsed = std::make_shared<Graphics::ParsedActionFile>(Graphics::ActParser::LoadFromFile(actPath));
    if (parsed->valid) {
        std::scoped_lock lock(m_assetMutex);
        if (m_actCache.size() >= 128) {
            auto it = m_actCache.begin();
            std::advance(it, m_actCache.size() / 2);
            m_actCache.erase(m_actCache.begin(), it);
        }
        m_actCache[key] = parsed;
    }
    return parsed;
}

std::shared_ptr<CharacterClassBundle> WorkspaceEngine::GetCharacterClass(CharacterClassId id) {
    if (id == CharacterClassId::ItemOnly) return nullptr;

    const int32_t key = static_cast<int32_t>(id);
    {
        std::scoped_lock lock(m_assetMutex);
        const auto it = m_charClassCache.find(key);
        if (it != m_charClassCache.end()) return it->second;
    }

    struct ClassDef {
        const char* name;
        const char* folder;
        const char* mesh;
        const char* skin;
        const char* ani;
        const char* act;
    };

    static const std::unordered_map<CharacterClassId, ClassDef> kDefs = {
        { CharacterClassId::Warrior,   { "Warrior",   "char/player/warrior",   "warrior.msh",   "warrior.skn",   "wa_normal.ani",     "warrior_basic.act" } },
        { CharacterClassId::Archer,    { "Archer",    "char/player/archer",    "archer.msh",    "archer.skn",    "archer_normal.ani", "archer_basic.act" } },
        { CharacterClassId::Sorceress, { "Sorceress", "char/player/soceress",  "soceress.msh",  "soceress.skn",  "so_normal.ani",     "soceress_basic.act" } },
        { CharacterClassId::Cleric,    { "Cleric",    "char/player/cleric",    "cleric.msh",    "cleric.skn",    "cl_normal.ani",     "cleric_basic.act" } },
        { CharacterClassId::Academic,  { "Academic",  "char/player/academic",  "academic.msh",  "academic.skn",  "ac_normal1.ani",    "academic_basic.act" } },
        { CharacterClassId::Kali,      { "Kali",      "char/player/kali",      "kali.msh",      "kali.skn",      "ka_normal.ani",     "kali_basic.act" } },
        { CharacterClassId::Assassin,  { "Assassin",  "char/player/assassin",  "assassin.msh",  "assassin.skn",  "as_normal.ani",     "assassin_basic.act" } },
        { CharacterClassId::Lancea,    { "Lancea",    "char/player/lencea",    "lencea.msh",    "lencea.skn",    "le_normal.ani",     "lencea_basic.act" } },
        { CharacterClassId::Machina,   { "Machina",   "char/player/machina",   "mc_basic.msh",  "mc_basic.skn",  "mc_normal.ani",     "machina_basic.act" } },
        { CharacterClassId::Vandar,    { "Vandar",    "char/player/vandar",    "vandar.msh",    "vandar.skn",    "va_normal.ani",     "vandar_basic.act" } },
    };

    auto itDef = kDefs.find(id);
    if (itDef == kDefs.end()) return nullptr;

    const auto& def = itDef->second;
    auto bundle = std::make_shared<CharacterClassBundle>();
    bundle->classId = id;
    bundle->className = def.name;

    const auto resRoot = m_items.GetResourcePath();
    if (resRoot.empty()) return nullptr;

    bundle->folderPath = resRoot / def.folder;
    const auto meshPath = bundle->folderPath / def.mesh;
    const auto skinPath = bundle->folderPath / def.skin;
    const auto aniPath  = bundle->folderPath / def.ani;
    const auto actPath  = bundle->folderPath / def.act;

    bundle->mesh = LoadMeshCached(meshPath);
    bundle->skin = LoadSkinCached(skinPath);
    bundle->animSet = LoadAniCached(aniPath);
    bundle->actions = LoadActCached(actPath);

    if (bundle->skin && bundle->skin->valid) {
        bundle->materials.resize(bundle->skin->subSkins.size());
        for (size_t i = 0; i < bundle->skin->subSkins.size(); ++i) {
            const auto& sub = bundle->skin->subSkins[i];
            auto& mat = bundle->materials[i];
            mat.emissivePower = sub.emissivePower;
            mat.alphaTest = !sub.alphaBlend;

            if (!sub.diffuseTexture.empty()) {
                const auto texPath = m_items.FindModelResource(sub.diffuseTexture, skinPath);
                if (!texPath.empty()) {
                    mat.diffuse = LoadTextureCached(texPath);
                }
            }
            if (!sub.emissiveTexture.empty()) {
                const auto emiPath = m_items.FindModelResource(sub.emissiveTexture, skinPath);
                if (!emiPath.empty()) {
                    mat.emissive = LoadTextureCached(emiPath);
                }
            }
        }
    }

    bundle->isLoaded = (bundle->mesh && bundle->mesh->valid && bundle->animSet && bundle->animSet->valid);

    {
        std::scoped_lock lock(m_assetMutex);
        m_charClassCache[key] = bundle;
    }
    return bundle;
}

CharacterClassId WorkspaceEngine::InferClassForItem(const Data::DNItem& item) const {
    // 1. Check skin / mesh filename prefix
    std::string skinName = m_items.FindSkinPath(item).filename().string();
    for (char& c : skinName) c = AsciiLower(c);

    if (skinName.starts_with("wa_") || skinName.starts_with("warrior")) return CharacterClassId::Warrior;
    if (skinName.starts_with("ar_") || skinName.starts_with("archer"))  return CharacterClassId::Archer;
    if (skinName.starts_with("so_") || skinName.starts_with("soceress")) return CharacterClassId::Sorceress;
    if (skinName.starts_with("cl_") || skinName.starts_with("cleric"))  return CharacterClassId::Cleric;
    if (skinName.starts_with("ac_") || skinName.starts_with("academic")) return CharacterClassId::Academic;
    if (skinName.starts_with("ka_") || skinName.starts_with("kali"))    return CharacterClassId::Kali;
    if (skinName.starts_with("as_") || skinName.starts_with("assassin")) return CharacterClassId::Assassin;
    if (skinName.starts_with("le_") || skinName.starts_with("lencea"))  return CharacterClassId::Lancea;
    if (skinName.starts_with("mc_") || skinName.starts_with("machina")) return CharacterClassId::Machina;
    if (skinName.starts_with("va_") || skinName.starts_with("vandar"))  return CharacterClassId::Vandar;

    // 2. Check jobClass string
    int j = 0;
    std::string_view jc = item.jobClass;
    if (auto semi = jc.find(';'); semi != std::string_view::npos) jc = jc.substr(0, semi);
    while (!jc.empty() && (jc.front() == ' ' || jc.front() == '\t')) jc.remove_prefix(1);
    if (!jc.empty()) {
        std::from_chars(jc.data(), jc.data() + jc.size(), j);
    }
    if (j == 1) return CharacterClassId::Warrior;
    if (j == 2) return CharacterClassId::Archer;
    if (j == 3) return CharacterClassId::Sorceress;
    if (j == 4) return CharacterClassId::Cleric;
    if (j == 5) return CharacterClassId::Academic;
    if (j == 6) return CharacterClassId::Kali;
    if (j == 7) return CharacterClassId::Assassin;
    if (j == 8) return CharacterClassId::Lancea;
    if (j == 9) return CharacterClassId::Machina;
    if (j == 10 || j == 201) return CharacterClassId::Vandar;

    if (j >= 11 && j <= 18) return CharacterClassId::Warrior;
    if (j >= 19 && j <= 26) return CharacterClassId::Archer;
    if (j >= 27 && j <= 34) return CharacterClassId::Sorceress;
    if (j >= 35 && j <= 42) return CharacterClassId::Cleric;
    if (j >= 43 && j <= 48) return CharacterClassId::Academic;
    if (j >= 49 && j <= 54) return CharacterClassId::Kali;
    if (j >= 56 && j <= 59) return CharacterClassId::Assassin;
    if (j >= 61 && j <= 64) return CharacterClassId::Lancea;
    if (j >= 65 && j <= 68) return CharacterClassId::Machina;
    if (j >= 69 && j <= 72) return CharacterClassId::Vandar;

    return CharacterClassId::ItemOnly;
}


bool WorkspaceEngine::SetCustomItemIcon(int32_t iconIndex, const Graphics::ImageBuffer& tile) {
    if (iconIndex < 0 || tile.width == 0 || tile.height == 0 || tile.rgbaData.empty()) {
        return false;
    }
    {
        std::scoped_lock lock(m_assetMutex);
        m_iconTileCache[iconIndex] = tile;
    }
    if (!m_items.GetResourcePath().empty()) {
        const auto customDir = m_items.GetResourcePath() / "ui" / "mainbar" / "custom";
        std::error_code ec;
        std::filesystem::create_directories(customDir, ec);
        const auto customPath = customDir / std::format("icon_{:05d}.dds", iconIndex);
        Graphics::DDSParser::SaveFileRGBA32(customPath, tile);
    }
    return true;
}

bool WorkspaceEngine::HasCustomItemIcon(int32_t iconIndex) const {
    if (iconIndex < 0) return false;
    if (!m_items.GetResourcePath().empty()) {
        const auto customPath = m_items.GetResourcePath() / "ui" / "mainbar" / "custom" / std::format("icon_{:05d}.dds", iconIndex);
        std::error_code ec;
        if (std::filesystem::exists(customPath, ec)) return true;
    }
    std::scoped_lock lock(m_assetMutex);
    return m_iconTileCache.contains(iconIndex);
}

} // namespace DNItemMaker::Engine
