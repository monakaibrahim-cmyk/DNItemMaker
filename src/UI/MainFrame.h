#pragma once

/**
 * @file MainFrame.h
 * @brief Studio-grade MFC main frame window hosting the complete item workshop.
 *
 * Implements the authentic native Windows MFC desktop interface, featuring:
 * - High-speed virtual list item browser with real-time substring search and multi-column sorting.
 * - 7-tab specialized workspace (Properties, Localization, Stats, 3D Model, Raw DNT, Automation, Tools).
 * - Live item summary banner with 50x50 icon well and MID collision diagnostics.
 * - Interactive software 3D preview viewport with mouse orbit, zoom, pan, and lighting.
 * - Transactional undo/redo stack, SQLite WAL recovery journaling, and folder-based saving.
 */

#include "UI/MfcCommon.h"
#include "UI/ModelPreviewWnd.h"
#include "UI/FlatControls.h"
#include "UI/Theme.h"
#include "Engine/WorkspaceEngine.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace DNItemMaker::UI {

/**
 * @class CIconSlotWnd
 * @brief Custom CWnd displaying a 50x50 DDS icon tile inside a flat single-pixel well.
 */
class CIconSlotWnd : public CWnd {
public:
    CIconSlotWnd() = default;

    /**
     * @brief Creates the icon slot control.
     * @param parent Pointer to parent window.
     * @param rect Bounding rectangle.
     * @param id Child window control ID.
     * @param clickCmdId Command ID posted to parent on left-click.
     * @return true on success.
     */
    bool Create(CWnd* parent, const CRect& rect, UINT id, UINT clickCmdId);

    /**
     * @brief Updates the displayed icon image buffer.
     * @param icon Pointer to decoded 50x50 ImageBuffer, or nullptr to clear.
     */
    void SetIcon(const Graphics::ImageBuffer* icon);

protected:
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);

    DECLARE_MESSAGE_MAP()

private:
    UINT m_clickCmdId = 0;
    bool m_hasIcon = false;
    Graphics::ImageBuffer m_icon;
};

/**
 * @class CMainFrame
 * @brief Main application frame window hosting the Dragon Nest Item Maker studio.
 */
class CMainFrame : public CFrameWnd {
public:
    static constexpr int kEditorTabCount = 8; ///< Total number of workspace tabs.

    CMainFrame();
    ~CMainFrame() override;

    /**
     * @brief Registers window class, constructs menus, accelerators, and displays the main window.
     * @return true on success.
     */
    bool CreateMainWindow();

    /**
     * @brief Loads the initial workspace with real-time feedback streamed to the Ghidra-style splash screen.
     * @param resourcePath Game resource folder path.
     * @param splash Pointer to active splash window.
     */
    void LoadInitialWorkspaceWithSplash(const std::filesystem::path& resourcePath, class CSplashWnd* splash);

    /** @brief Provides direct mutable access to the workspace engine. */
    Engine::WorkspaceEngine& GetEngine() noexcept { return m_engine; }

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL PreTranslateMessage(MSG* pMsg) override;

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    afx_msg LRESULT OnWorkspaceLoadFinished(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnModelIndexReady(WPARAM wParam, LPARAM lParam);

    afx_msg void OnFileOpenWorkspace();
    afx_msg void OnFileReloadWorkspace();
    afx_msg void OnFileSaveToFolder();
    afx_msg void OnFileCommitToDisk();
    afx_msg void OnFileDiscardDrafts();
    afx_msg void OnAppExit();

    afx_msg void OnItemNew();
    afx_msg void OnItemDuplicate();
    afx_msg void OnItemApply();
    afx_msg void OnItemRevert();
    afx_msg void OnItemDelete();
    afx_msg void OnItemUndo();
    afx_msg void OnItemPickIcon();
    afx_msg void OnFocusSearch();

    afx_msg void OnToolsVerifyRoundTrip();
    afx_msg void OnToolsWorkspaceAudit();
    afx_msg void OnToolsClearCache();
    afx_msg void OnToolsCaptureSnapshot();
    afx_msg void OnToolsOpenLogsFolder();
    afx_msg void OnToolsDragonJewelStudio();
    afx_msg void OnToolsSetItemStudio();
    afx_msg void OnToolsEnchantStudio();
    afx_msg void OnHelpAbout();

    afx_msg void OnSearchTextChanged();
    afx_msg void OnClearSearch();
    afx_msg void OnFilterChanged();
    afx_msg void OnItemListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemListCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemListFocusChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemListSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemListColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemListRightClick(NMHDR* pNMHDR, LRESULT* pResult);

    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTabShortcut(UINT nID);

    afx_msg void OnRankComboChanged();
    afx_msg void OnIconIndexEditChanged();
    afx_msg void OnJobsEditChanged();
    afx_msg void OnClassMatrixClicked(UINT nID);
    afx_msg void OnAutoCalcSell();
    afx_msg void OnAutoCalcGearScore();
    afx_msg void OnAutoCalcDurability();
    afx_msg void OnAutoCalcAllEconomy();
    afx_msg void OnPresetEquipDefaults();
    afx_msg void OnPresetStack999();
    afx_msg void OnPresetCashBound();

    afx_msg void OnLiveTextPreviewChanged();
    afx_msg void OnAllocNameMid();
    afx_msg void OnAllocDescMid();
    afx_msg void OnForkExclusiveMids();
    afx_msg void OnSearchNameUIString();
    afx_msg void OnSearchDescUIString();
    afx_msg void OnGenerateRichTooltip();
    afx_msg void OnInsertColorTag(UINT nID);

    afx_msg void OnStatsApplyArchetype();
    afx_msg void OnStatsAutoCurve();
    afx_msg void OnStatsScaleButton(UINT nID);
    afx_msg void OnStatsSpread15();
    afx_msg void OnStatsCopyMinToMax();
    afx_msg void OnStatsProbsFull();
    afx_msg void OnStatsCompact();
    afx_msg void OnStatsCopyClipboard();
    afx_msg void OnStatsPasteClipboard();
    afx_msg void OnStatsClearUnused();

    afx_msg void OnRenderModeChanged();
    afx_msg void OnToggleAutoSpin();
    afx_msg void OnZoomIn3D();
    afx_msg void OnZoomOut3D();
    afx_msg void OnResetCamera();
    afx_msg void OnUseSelectedAsDonor();
    afx_msg void OnCopyVisualBoth();
    afx_msg void OnCopyVisualSkinOnly();
    afx_msg void OnCopyVisualIconOnly();

    afx_msg void OnRawTableSourceChanged();
    afx_msg void OnRawFilterChanged();
    afx_msg void OnRawListSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRawListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRawWriteCell();
    afx_msg void OnRawCopyRowTsv();
    afx_msg void OnRawCloneDetailFromDonor();

    afx_msg void OnGenerateTierSet();
    afx_msg void OnGenerateClassSeries();
    afx_msg void OnExecuteBatchEdit();

    afx_msg void OnExportSelectedJson();
    afx_msg void OnExportModifiedJson();
    afx_msg void OnExportFilteredJson();
    afx_msg void OnImportJsonFile();
    afx_msg void OnImportJsonClipboard();
    afx_msg void OnRefreshModifiedList();
    afx_msg void OnModifiedListDblClick(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    struct RawColumnEntry {
        size_t colIdx = 0;
        std::string name;
        std::string typeName;
        std::string value;
    };

    struct StatRowWidgets {
        CStatic lblSlot;
        CComboBox cmbType;
        CFlatEdit edtProb;
        CFlatEdit edtMin;
        CFlatEdit edtMax;
        int32_t preservedUnknownType = -1;
    };

    void BuildMenusAndAccelerators();
    void CreateChildControls();
    void LayoutControls(int cx, int cy);
    void ShowActiveEditorTab(int tabIdx);
    void UpdateVisibleTabsForItem(const Data::DNItem* item);
    void RebuildTabControl();
    std::wstring GetLogicalTabTitle(int logIdx, int displayNum) const;

    void PopulateCharmTabForCurrentItem();
    void PopulateCharmRewardsList(int32_t charmNum);
    void UpdateCharmStatsAndAnalytics(const Data::CharmGroup& group);
    void RunGachaSimulation(int pullCount, bool untilJackpot = false);
    void UpdateGachaSimDisplay();

    afx_msg void OnToolsCharmCalculator();
    afx_msg void OnCharmSelectorChange();
    afx_msg void OnCharmSyncCurrentItem();
    afx_msg void OnCharmRefresh();
    afx_msg void OnCharmExportCsv();
    afx_msg void OnCharmRewardItemSelect(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnCharmNormalizeWeights();
    afx_msg void OnCharmApplySelectedWeight();
    afx_msg void OnSimPull1() { RunGachaSimulation(1); }
    afx_msg void OnSimPull10() { RunGachaSimulation(10); }
    afx_msg void OnSimPull100() { RunGachaSimulation(100); }
    afx_msg void OnSimPull1000() { RunGachaSimulation(1000); }
    afx_msg void OnSimJackpot() { RunGachaSimulation(1, true); }
    afx_msg void OnSimReset();

    void StartAsyncWorkspaceLoad(const std::filesystem::path& resourcePath);
    void RefreshTableFilterCombo();
    void UpdateBrowserSortHeaders();
    void RunItemSearch(int32_t preserveSelectedId = -1);
    void SelectAndRevealItem(int32_t itemId);

    void LoadItemIntoEditor(const Data::DNItem* item);
    bool CollectItemFromEditor(Data::DNItem& outItem, std::string& outError) const;
    void PopulateStatWidgetsFromItem(const Data::DNItem& item);
    void SyncClassCheckboxesFromJobString(std::string_view jobStr);
    void UpdateMidCollisionBanner();
    void UpdateLiveResolvedPreviews();
    void PopulateRawInspectorList();
    void RefreshModifiedItemsTabList();
    void ExportItemPackInteractive(const std::vector<int32_t>& ids, const std::string& defaultFileName);

    void UpdateItemSummary(const Data::DNItem* item,
                           const std::string& sourceTableName,
                           const std::string& detailTableName,
                           const Graphics::ImageBuffer* iconImg,
                           bool isModified,
                           size_t sharedNameMidCount);
    void UpdateLiveSummaryPreview(const std::string& liveName,
                                 int32_t liveRank,
                                 int32_t liveIconIdx,
                                 const Graphics::ImageBuffer* iconImg,
                                 size_t sharedNameMidCount);
    void ClearItemSummary();

    void UpdateStatusBar(const std::string& actionMsg = {});
    void UpdateWindowCaption();
    void SetWorkspaceControlsEnabled(bool enabled);

    void CopyVisualHelper(bool copySkin, bool copyIcon, const char* label);

    template <typename Fn>
    void MutateCurrentItem(Fn&& fn, const char* statusMsg = nullptr) {
        Data::DNItem item;
        std::string err;
        if (!CollectItemFromEditor(item, err)) return;
        fn(item);
        PopulateStatWidgetsFromItem(item);
        if (m_chkAutoApply.GetCheck() == BST_CHECKED) {
            OnItemApply();
        }
        if (statusMsg) {
            UpdateStatusBar(statusMsg);
        }
    }

    Engine::WorkspaceEngine m_engine;
    std::jthread m_loadWorker;
    std::atomic<bool> m_loadingWorkspace{ false };
    std::filesystem::path m_pendingPath;
    size_t m_lastRecoveredDrafts = 0;

    std::vector<const Data::DNItem*> m_filteredItems;
    Data::ItemSortColumn m_sortBy = Data::ItemSortColumn::Id;
    bool m_sortAscending = true;

    int32_t m_selectedItemId = 0;
    int32_t m_visualDonorItemId = 0;
    bool m_populatingEditor = false;
    bool m_syncingClasses = false;
    int m_activeTab = 0;

    std::vector<int> m_visibleTabs;
    int32_t m_currentCharmNum = 0;
    int32_t m_selectedCharmRowId = 0;

    struct GachaSimState {
        int64_t totalPulls = 0;
        int64_t totalGoldWon = 0;
        int64_t jackpotsHit = 0;
        std::unordered_map<int32_t, int64_t> itemHits;
        std::deque<std::string> recentPulls;
    };
    GachaSimState m_simState;

    bool m_hasStatClipboard = false;
    std::array<Data::StateSlot, Data::kStateSlotCount> m_statClipboard{};

    std::vector<RawColumnEntry> m_rawEntries;
    int m_selectedRawColIdx = -1;
    std::vector<int32_t> m_modifiedListIds;

    CRect m_rcTopHeader{};
    CRect m_rcLeftCard{};
    CRect m_rcRightCard{};

    HICON m_hAppIcon = nullptr;
    HACCEL m_hAccel = nullptr;
    Theme::FontSet m_fonts;

    CStatusBar m_statusBar;

    // Top Command Strip Buttons
    CFlatButton m_btnHdrOpen;
    CFlatButton m_btnHdrNew;
    CFlatButton m_btnHdrDup;
    CFlatButton m_btnHdrUndo;
    CFlatButton m_btnHdrSaveToFolder;
    CFlatButton m_btnHdrCommit;
    CFlatButton m_btnHdrAudit;
    CFlatButton m_btnHdrVerify;

    // Classic Windows Etched Group Boxes
    CFlatGroupBox m_grpBrowser;
    CFlatGroupBox m_grpSummary;

    // Tab 0 (Properties)
    CFlatGroupBox m_grpBasicProps;
    CFlatGroupBox m_grpEconProps;
    CFlatGroupBox m_grpJobs;

    // Tab 1 (Localization)
    CFlatGroupBox m_grpNameMid;
    CFlatGroupBox m_grpDescMid;
    CFlatGroupBox m_grpColorTags;

    // Tab 2 (Stats)
    CFlatGroupBox m_grpArchetypes;
    CFlatGroupBox m_grpStateRows;

    // Tab 3 (3D Model)
    CFlatGroupBox m_grpViewport;
    CFlatGroupBox m_grpVisualCloner;

    // Tab 4 (Raw DNT)
    CFlatGroupBox m_grpRawInspector;
    CFlatGroupBox m_grpRawEdit;

    // Tab 5 (Automation)
    CFlatGroupBox m_grpTierGen;
    CFlatGroupBox m_grpSeriesGen;
    CFlatGroupBox m_grpBatchEdit;

    // Tab 6 (Tools)
    CFlatGroupBox m_grpTools;
    CFlatGroupBox m_grpJsonPacks;
    CFlatGroupBox m_grpModDrafts;

    // Tab 7 (Charm / Gacha Box Probability Studio)
    CFlatGroupBox m_grpCharmHeader;
    CFlatGroupBox m_grpCharmTable;
    CFlatGroupBox m_grpCharmStats;
    CFlatGroupBox m_grpCharmSim;
    CFlatGroupBox m_grpCharmBalance;

    // Bottom Action Frame
    CFlatGroupBox m_grpBottomActions;

    // Left Pane Controls (Item Browser)
    CFlatEdit m_edtSearch;
    CFlatButton m_btnClearSearch;
    CComboBox m_cmbTableFilter;
    CComboBox m_cmbGroupFilter;
    CComboBox m_cmbRankFilter;
    CButton m_chkModifiedOnly;
    CListCtrl m_lstItems;
    CStatic m_lblMatchCount;
    CFlatButton m_btnNewItem;
    CFlatButton m_btnDupItem;
    CFlatButton m_btnDelItem;

    // Right Top: Item Summary Controls
    CIconSlotWnd m_bannerIconWnd;
    CStatic m_lblBannerName;
    CStatic m_lblBannerMeta;
    CStatic m_lblBannerMid;
    CFlatButton m_btnBannerPickIcon;

    // Standard Tab Control
    CFlatTabCtrl m_tabCtrl;
    std::array<std::vector<CWnd*>, kEditorTabCount> m_tabControls;

    // Tab 0: Core Attributes
    CStatic m_lblRank, m_lblIconIdx, m_lblLevel, m_lblHeroLevel, m_lblOverlap, m_lblSell, m_lblGearScore, m_lblDurability;
    CComboBox m_cmbEditRank;
    CFlatEdit m_edtIconIndex;
    CFlatButton m_btnBrowseIcon;
    CFlatEdit m_edtLevel, m_edtHeroLevel, m_edtOverlap, m_edtSell, m_edtGearScore, m_edtDurability;
    CFlatButton m_btnAutoSell, m_btnAutoGearScore, m_btnAutoDurability, m_btnAutoAllEconomy;
    CButton m_chkCash, m_chkStorage, m_chkWStorage;
    CFlatButton m_btnPresetEquip, m_btnPresetStack, m_btnPresetCash;
    CStatic m_lblPropJewelInfo;
    CFlatButton m_btnPropJewelStudio;
    CStatic m_lblPropSetItemInfo;
    CFlatButton m_btnPropSetItemStudio;
    CStatic m_lblPropEnchantInfo;
    CFlatButton m_btnPropEnchantStudio;

    CStatic m_lblJobs, m_lblJobsResolved;
    CFlatEdit m_edtJobs;
    CButton m_chkClassAll;
    std::array<CButton, 10> m_chkBaseClasses;

    // Tab 1: Localization
    CStatic m_lblMidCollisionInfo;
    CFlatButton m_btnForkMids;
    CFlatButton m_btnGenTooltip;

    CStatic m_lblNameMid, m_lblNameParam, m_lblNameTmpl, m_lblNameResolvedHdr;
    CFlatEdit m_edtNameMid, m_edtNameParam, m_edtNameTmpl, m_edtNameResolved;
    CFlatButton m_btnAllocNameMid, m_btnSearchNameMid;

    CStatic m_lblDescMid, m_lblDescParam, m_lblDescTmpl, m_lblDescResolvedHdr;
    CFlatEdit m_edtDescMid, m_edtDescParam, m_edtDescTmpl, m_edtDescResolved;
    CFlatButton m_btnAllocDescMid, m_btnSearchDescMid;
    std::array<CFlatButton, 7> m_btnColorTags;

    // Tab 2: Stats
    CStatic m_lblArchetype;
    CComboBox m_cmbArchetype;
    CFlatButton m_btnApplyArchetype;
    CFlatButton m_btnStatsAutoCurve;
    std::array<CFlatButton, 5> m_btnStatScales;
    CFlatButton m_btnStatsSpread15;
    CFlatButton m_btnStatsMinToMax;
    CFlatButton m_btnStatsProbsFull;
    CFlatButton m_btnStatsCompact;
    CFlatButton m_btnStatsCopy;
    CFlatButton m_btnStatsPaste;
    CFlatButton m_btnStatsClear;

    CStatic m_lblStatHdrSlot, m_lblStatHdrType, m_lblStatHdrProb, m_lblStatHdrMin, m_lblStatHdrMax;
    std::array<StatRowWidgets, Data::kStateSlotCount> m_statWidgets;

    // Tab 3: 3D Viewport
    CStatic m_lblRenderMode;
    CComboBox m_cmbRenderMode;
    CButton m_chkAutoSpin;
    CFlatButton m_btnZoomIn3D;
    CFlatButton m_btnZoomOut3D;
    CFlatButton m_btnResetCam;
    CFlatButton m_btnOpenIconAtlas;
    CModelPreviewWnd m_previewWnd;

    CStatic m_lblDonorId;
    CFlatEdit m_edtDonorId;
    CFlatButton m_btnUseSelAsDonor;
    CFlatButton m_btnCloneVisualBoth;
    CFlatButton m_btnCloneSkinOnly;
    CFlatButton m_btnCloneIconOnly;

    // Tab 4: Raw Inspector
    CStatic m_lblRawSource;
    CComboBox m_cmbRawSource;
    CFlatEdit m_edtRawFilter;
    CFlatButton m_btnRawCopyTsv;
    CFlatButton m_btnRawCloneDetail;
    CListCtrl m_lstRawCols;
    CStatic m_lblRawSelectedCol;
    CFlatEdit m_edtRawCellValue;
    CFlatButton m_btnRawWriteCell;

    // Tab 5: Automation
    CStatic m_lblTierBaseName, m_lblTierGrowth;
    CFlatEdit m_edtTierBaseName, m_edtTierGrowth;
    std::array<CButton, 7> m_chkTierRanks;
    CButton m_chkTierAutoCurve, m_chkTierAutoEcon, m_chkTierRichDesc;
    CFlatButton m_btnRunTierSet;

    CStatic m_lblSeriesTmpl;
    CFlatEdit m_edtSeriesTmpl;
    std::array<CButton, 10> m_chkSeriesClasses;
    CButton m_chkSeriesAdaptStats, m_chkSeriesRichDesc;
    CFlatButton m_btnRunClassSeries;

    CStatic m_lblBatchTargetInfo;
    CButton m_chkBatchScaleStats, m_chkBatchAutoCurve, m_chkBatchSanitize, m_chkBatchAutoEcon;
    CFlatEdit m_edtBatchScaleFactor;
    CButton m_chkBatchSetRank, m_chkBatchSetLevel, m_chkBatchSetOverlap;
    CComboBox m_cmbBatchRank;
    CFlatEdit m_edtBatchLevel, m_edtBatchOverlap;
    CStatic m_lblBatchPrefix, m_lblBatchSuffix;
    CFlatEdit m_edtBatchPrefix, m_edtBatchSuffix;
    CFlatButton m_btnRunBatchEdit;

    // Tab 6: Tools & Packs
    CFlatButton m_btnToolAudit, m_btnToolVerify, m_btnToolClearCache, m_btnToolSnapshot, m_btnToolLogs;
    CFlatButton m_btnJsonExpSel, m_btnJsonExpMod, m_btnJsonExpFilt, m_btnJsonImpFile, m_btnJsonImpClip;
    CListCtrl m_lstModifiedItems;
    CFlatButton m_btnModRefresh;
    CFlatButton m_btnModUndo;

    CFlatButton m_btnLaunchCharmCalc;
    CFlatButton m_btnOpenDragonJewelStudio;
    CFlatButton m_btnOpenSetItemStudio;
    CFlatButton m_btnOpenEnchantStudio;

    // Tab 7: Charm & Gacha Box Probability Studio
    CStatic m_lblCharmSelector;
    CComboBox m_cmbCharmSelector;
    CFlatButton m_btnCharmSyncItem;
    CFlatButton m_btnCharmRefresh;
    CFlatButton m_btnCharmExportCsv;

    CListCtrl m_lstCharmRewards;

    CStatic m_lblCharmStatSummary;
    CStatic m_lblCharmStatOdds;

    CStatic m_lblSimTitle;
    CFlatButton m_btnSimPull1;
    CFlatButton m_btnSimPull10;
    CFlatButton m_btnSimPull100;
    CFlatButton m_btnSimPull1000;
    CFlatButton m_btnSimJackpot;
    CFlatButton m_btnSimReset;
    CFlatEdit m_edtSimLog;

    CStatic m_lblCharmTargetWeight;
    CFlatEdit m_edtCharmTargetWeight;
    CFlatButton m_btnCharmNormalize;
    CStatic m_lblCharmEditWeight;
    CFlatEdit m_edtCharmEditWeight;
    CFlatButton m_btnCharmApplyWeight;

    // Bottom Action Row
    CButton m_chkAutoApply;
    CFlatButton m_btnApplyItem;
    CFlatButton m_btnUndoBottom;
    CFlatButton m_btnRevertItem;
    CFlatButton m_btnSaveToFolder;
    CFlatButton m_btnCommitDisk;
};

} // namespace DNItemMaker::UI
