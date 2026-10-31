#pragma once

/**
 * @file ModalDialogs.h
 * @brief Native MFC modal dialogs for icon selection, item creation, string lookup, and about.
 *
 * Implements dialog interfaces using runtime memory dialog templates (EmptyDialogTemplate)
 * and native Windows controls:
 * - CIconPickerDlg: Interactive 2D DDS icon atlas browser with paginated grid and live zoom.
 * - CNewItemDlg: Item creation and duplication wizard with auto-suggested IDs.
 * - CUIStringSearchDlg: Fast substring search and MID selection dialog for `uistring.xml`.
 * - CReportDlg: Monospace text report viewer for audits, round-trip checks, and batch summaries.
 * - CAboutDlg: Application identity dialog displaying build timestamp, target architecture, and copyright.
 */

#include "UI/MfcCommon.h"
#include "UI/FlatControls.h"
#include "UI/Theme.h"
#include "Engine/WorkspaceEngine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace DNItemMaker::UI {

/**
 * @class CThemedDialog
 * @brief CDialog base that routes WM_CTLCOLOR* through the shared Theme palette.
 */
class CThemedDialog : public CDialog {
protected:
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    DECLARE_MESSAGE_MAP()

    Theme::FontSet m_fonts;
};

/**
 * @class CIconPickerDlg
 * @brief Modal dialog for visually selecting icon indices from DDS atlas pages.
 */
class CIconPickerDlg : public CThemedDialog {
public:
    /**
     * @brief Constructs the icon picker dialog.
     * @param engine Reference to active WorkspaceEngine.
     * @param initialIconIndex Currently selected icon index to highlight.
     * @param parent Optional parent window.
     */
    CIconPickerDlg(Engine::WorkspaceEngine& engine, int32_t initialIconIndex, CWnd* parent = nullptr);

    /** @brief Returns the chosen 0-based icon atlas index. */
    int32_t SelectedIconIndex() const noexcept { return m_selectedIndex; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnPrevPage();
    afx_msg void OnNextPage();
    afx_msg void OnPageComboChanged();
    afx_msg void OnJumpToIndex();

    DECLARE_MESSAGE_MAP()

private:
    void LoadPage(int page);
    int TileFromPoint(CPoint pt) const;
    void UpdateInfoText();

    EmptyDialogTemplate m_tmpl;
    Engine::WorkspaceEngine& m_engine;
    int32_t m_selectedIndex = 0;
    int m_currentPage = 1;
    int m_hoverTile = -1;

    std::shared_ptr<const Graphics::ImageBuffer> m_atlas;
    Graphics::ImageBuffer m_croppedAtlasGrid;
    Graphics::ImageBuffer m_selectedTileImg;
    bool m_hasSelectedTileImg = false;

    CRect m_gridRect{};
    CRect m_zoomRect{};

    CFlatButton m_btnPrev;
    CFlatButton m_btnNext;
    CComboBox m_cmbPage;
    CFlatEdit m_edtDirectIndex;
    CFlatButton m_btnJump;
    CStatic m_lblInfo;
    CFlatButton m_btnOk;
    CFlatButton m_btnCancel;
};

/**
 * @class CNewItemDlg
 * @brief Modal dialog for creating brand-new items or cloning existing items.
 */
class CNewItemDlg : public CThemedDialog {
public:
    /**
     * @brief Constructs the new item dialog.
     * @param engine Reference to active WorkspaceEngine.
     * @param duplicateMode True for cloning an existing item, false for new item from scratch.
     * @param defaultTableIndex Initially selected table partition index.
     * @param sourceItemId Source item ID when duplicating.
     * @param defaultName Default initial name string.
     * @param parent Optional parent window.
     */
    CNewItemDlg(const Engine::WorkspaceEngine& engine,
                bool duplicateMode,
                int defaultTableIndex,
                int32_t sourceItemId = 0,
                const std::string& defaultName = {},
                CWnd* parent = nullptr);

    /** @brief Target partition table index selected by user. */
    int SelectedTableIndex() const noexcept { return m_resultTableIndex; }

    /** @brief 32-bit item ID assigned or entered by user. */
    int32_t SelectedItemId() const noexcept { return m_resultItemId; }

    /** @brief Raw engine item type enum chosen. */
    int32_t SelectedRawType() const noexcept { return m_resultRawType; }

    /** @brief Equipment slot subtype chosen (-1 if not equipment). */
    int32_t SelectedEquipSubType() const noexcept { return m_resultEquipSubType; }

    /** @brief Item display name entered. */
    const std::string& SelectedName() const noexcept { return m_resultName; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnTableChanged();
    afx_msg void OnTypeChanged();
    afx_msg void OnSuggestId();

    DECLARE_MESSAGE_MAP()

private:
    void PopulateSubTypes(int32_t rawType);

    EmptyDialogTemplate m_tmpl;
    const Engine::WorkspaceEngine& m_engine;
    bool m_duplicateMode = false;
    int m_defaultTableIndex = 0;
    int32_t m_sourceItemId = 0;
    std::string m_defaultName;

    int m_resultTableIndex = 0;
    int32_t m_resultItemId = 0;
    int32_t m_resultRawType = 0;
    int32_t m_resultEquipSubType = -1;
    std::string m_resultName;

    CStatic m_lblHeader;
    CStatic m_lblName;
    CFlatEdit m_edtName;
    CStatic m_lblTable;
    CComboBox m_cmbTable;
    CStatic m_lblId;
    CFlatEdit m_edtId;
    CFlatButton m_btnSuggestId;
    CStatic m_lblType;
    CComboBox m_cmbType;
    CStatic m_lblSubType;
    CComboBox m_cmbSubType;
    CFlatButton m_btnOk;
    CFlatButton m_btnCancel;
};

/**
 * @class CUIStringSearchDlg
 * @brief Modal dialog for querying `uistring.xml` and selecting Message IDs.
 */
class CUIStringSearchDlg : public CThemedDialog {
public:
    /**
     * @brief Constructs the UIString search dialog.
     * @param engine Reference to active WorkspaceEngine.
     * @param initialQuery Initial search substring.
     * @param parent Optional parent window.
     */
    CUIStringSearchDlg(const Engine::WorkspaceEngine& engine,
                       std::string initialQuery = {},
                       CWnd* parent = nullptr);

    /** @brief Returns the chosen Message ID (MID). */
    uint32_t SelectedMid() const noexcept { return m_selectedMid; }

    /** @brief Returns the localized text of the selected Message ID. */
    const std::string& SelectedText() const noexcept { return m_selectedText; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnQueryChanged();
    afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    void RunSearch();

    EmptyDialogTemplate m_tmpl;
    const Engine::WorkspaceEngine& m_engine;
    std::string m_initialQuery;
    std::vector<Data::UIStringHit> m_hits;

    uint32_t m_selectedMid = 0;
    std::string m_selectedText;

    CStatic m_lblSearch;
    CFlatEdit m_edtQuery;
    CStatic m_lblCount;
    CListCtrl m_lstHits;
    CFlatButton m_btnOk;
    CFlatButton m_btnCancel;
};

/**
 * @class CReportDlg
 * @brief Monospace report viewer dialog with clipboard copy and file export.
 */
class CReportDlg : public CThemedDialog {
public:
    /**
     * @brief Constructs the report dialog.
     * @param title Dialog window title text.
     * @param summaryHeader Single-line summary header.
     * @param bodyText Detailed report contents.
     * @param parent Optional parent window.
     */
    CReportDlg(std::string title, std::string summaryHeader, std::string bodyText, CWnd* parent = nullptr);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnCopyClipboard();
    afx_msg void OnSaveToFile();

    DECLARE_MESSAGE_MAP()

private:
    EmptyDialogTemplate m_tmpl;
    std::string m_summaryHeader;
    std::string m_bodyText;

    CStatic m_lblHeader;
    CFlatEdit m_edtReport;
    CFlatButton m_btnCopy;
    CFlatButton m_btnSave;
    CFlatButton m_btnClose;
};

/**
 * @class CAboutDlg
 * @brief Standard application About dialog displaying program identity, build date, and architecture.
 */
class CAboutDlg : public CThemedDialog {
public:
    /**
     * @brief Constructs the about dialog.
     * @param parent Optional parent window.
     */
    explicit CAboutDlg(CWnd* parent = nullptr);

protected:
    BOOL OnInitDialog() override;

private:
    EmptyDialogTemplate m_tmpl;
    CStatic m_lblIcon;
    CStatic m_lblTitle;
    CStatic m_lblVersion;
    CStatic m_lblBuildDate;
    CStatic m_lblArch;
    CStatic m_lblCopyright;
    CFlatButton m_btnOk;
};


/**
 * @class CDragonJewelStudioDlg
 * @brief Studio dialog for inspecting/editing Dragon Jewel sockets and simulating jade stats.
 */
class CDragonJewelStudioDlg : public CThemedDialog {
public:
    CDragonJewelStudioDlg(Engine::WorkspaceEngine& engine, int32_t currentItemId, CWnd* parent = nullptr);

    int32_t SelectedItemId() const noexcept { return m_selectedItemId; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnTypeFilterChanged();
    afx_msg void OnRankFilterChanged();
    afx_msg void OnJewelItemSelect(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnApplySocketConfig();
    afx_msg void OnSocketSlot1();
    afx_msg void OnSocketSlot2();
    afx_msg void OnSocketSlot3();
    afx_msg void OnSocketSlot4();
    afx_msg void OnClearAllSockets();
    afx_msg void OnEquipItemSelect(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    void RefreshJewelList();
    void RefreshSocketView();
    void RefreshNetStats();
    void RefreshCompatibleEquipList();

    Engine::WorkspaceEngine& m_engine;
    int32_t m_selectedItemId = 0;
    int32_t m_currentConfigId = 0;
    std::array<int32_t, 4> m_socketedJewelIds{ 0, 0, 0, 0 };
    int32_t m_selectedListJewelId = 0;
    bool m_isJewelItem = false;

    EmptyDialogTemplate m_tmpl;
    CStatic m_lblHeader;
    CFlatGroupBox m_grpSockets;
    CStatic m_lblConfigId;
    CFlatEdit m_edtConfigId;
    CFlatButton m_btnApplyConfig;
    CStatic m_lblSlotInfo[4];
    CFlatButton m_btnSocket[4];
    CFlatButton m_btnClearSockets;

    CFlatGroupBox m_grpBrowser;
    CStatic m_lblTypeFilter;
    CComboBox m_cmbTypeFilter;
    CStatic m_lblRankFilter;
    CComboBox m_cmbRankFilter;
    CListCtrl m_lstJewels;

    CFlatGroupBox m_grpNetStats;
    CStatic m_lblNetStats;

    CFlatGroupBox m_grpEquip;
    CListCtrl m_lstEquip;

    CFlatButton m_btnClose;
};

/**
 * @class CSetItemStudioDlg
 * @brief Studio dialog for inspecting all Set Items, equipment set pieces, and tiered bonuses.
 */
class CSetItemStudioDlg : public CThemedDialog {
public:
    CSetItemStudioDlg(Engine::WorkspaceEngine& engine, int32_t initialSetId = 0, CWnd* parent = nullptr);

    int32_t JumpToItemId() const noexcept { return m_jumpToItemId; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnSearchSetTextChanged();
    afx_msg void OnSetSelect(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnPieceItemClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnPieceItemDblClk(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnToggleAllPieces();
    afx_msg void OnJumpToItem();

    DECLARE_MESSAGE_MAP()

private:
    void PopulateSetList(std::string_view filter);
    void DisplaySet(int32_t setId);
    void UpdateSimulatorTotal();

    Engine::WorkspaceEngine& m_engine;
    int32_t m_currentSetId = 0;
    int32_t m_jumpToItemId = 0;
    std::vector<bool> m_pieceEquipped;
    std::vector<int32_t> m_currentMemberIds;

    EmptyDialogTemplate m_tmpl;
    CStatic m_lblHeader;

    CFlatGroupBox m_grpSets;
    CFlatEdit m_edtSearchSet;
    CListCtrl m_lstSets;

    CFlatGroupBox m_grpPieces;
    CListCtrl m_lstPieces;
    CFlatButton m_btnToggleAll;

    CFlatGroupBox m_grpBonuses;
    CListCtrl m_lstBonuses;
    CStatic m_lblSkillBonus;

    CFlatGroupBox m_grpSimSummary;
    CStatic m_lblSimSummary;

    CFlatButton m_btnJumpToItem;
    CFlatButton m_btnClose;
};

/**
 * @class CEnchantStudioDlg
 * @brief Studio dialog for viewing +1..+20 enchant progression curves and running Monte Carlo simulations.
 */
class CEnchantStudioDlg : public CThemedDialog {
public:
    CEnchantStudioDlg(Engine::WorkspaceEngine& engine, int32_t enchantId, int32_t currentItemId, CWnd* parent = nullptr);

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnRunSimulation();

    DECLARE_MESSAGE_MAP()

private:
    void PopulateCurveTable();
    void RunMonteCarlo(int trials, int targetLvl, bool useJelly);

    Engine::WorkspaceEngine& m_engine;
    int32_t m_enchantId = 0;
    int32_t m_itemId = 0;
    Data::EnchantCurve m_curve;

    EmptyDialogTemplate m_tmpl;
    CStatic m_lblHeader;

    CFlatGroupBox m_grpTable;
    CListCtrl m_lstSteps;

    CFlatGroupBox m_grpSim;
    CStatic m_lblTargetLvl;
    CComboBox m_cmbTargetLvl;
    CButton m_chkJelly;
    CStatic m_lblTrials;
    CComboBox m_cmbTrials;
    CFlatButton m_btnRunSim;
    CFlatEdit m_edtTelemetry;

    CFlatButton m_btnClose;
};

} // namespace DNItemMaker::UI
