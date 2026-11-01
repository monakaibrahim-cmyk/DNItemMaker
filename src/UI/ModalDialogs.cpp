#include "Graphics/ActConverter.h"
#include "Graphics/DDSParser.h"
#include <random>
#include "UI/ModalDialogs.h"
#include "version.h"

#include <algorithm>
#include <format>
#include <fstream>

namespace DNItemMaker::UI {

namespace {

enum DialogCtrlIds : UINT {
    IDC_ICON_PREV = 3101,
    IDC_ICON_NEXT,
    IDC_ICON_PAGE_COMBO,
    IDC_ICON_DIRECT_EDIT,
    IDC_ICON_JUMP_BTN,
    IDC_ICON_INFO_LBL,

    IDC_NEW_NAME_EDIT = 3201,
    IDC_NEW_TABLE_COMBO,
    IDC_NEW_ID_EDIT,
    IDC_NEW_ID_SUGGEST,
    IDC_NEW_TYPE_COMBO,
    IDC_NEW_SUBTYPE_COMBO,

    IDC_REPORT_EDIT = 3301,
    IDC_REPORT_COPY,
    IDC_REPORT_SAVE,

    IDC_UISTR_QUERY = 3401,
    IDC_UISTR_COUNT,
    IDC_UISTR_LIST,

    IDC_DJ_APPLY_CFG = 3501,
    IDC_DJ_TYPE_COMBO,
    IDC_DJ_RANK_COMBO,
    IDC_DJ_JEWEL_LIST,
    IDC_DJ_EQUIP_LIST,
    IDC_DJ_SOCKET_1,
    IDC_DJ_SOCKET_2,
    IDC_DJ_SOCKET_3,
    IDC_DJ_SOCKET_4,
    IDC_DJ_CLEAR_ALL,

    IDC_SET_SEARCH = 3601,
    IDC_SET_LIST,
    IDC_SET_PIECES_LIST,
    IDC_SET_BONUSES_LIST,
    IDC_SET_TOGGLE_ALL,
    IDC_SET_JUMP_BTN,

    IDC_ENC_STEPS_LIST = 3701,
    IDC_ENC_TARGET_LVL,
    IDC_ENC_JELLY_CHK,
    IDC_ENC_TRIALS_COMBO,
    IDC_ENC_RUN_BTN,
    IDC_ENC_TELEMETRY,

    IDC_ACT_INPUT_EDIT = 3801,
    IDC_ACT_BROWSE_IN,
    IDC_ACT_CONVERT_SINGLE,
    IDC_ACT_BATCH_EDIT,
    IDC_ACT_BROWSE_BATCH,
    IDC_ACT_BACKUP_CHK,
    IDC_ACT_CONVERT_BATCH,
    IDC_ACT_LOG,

    IDC_ICON_TOOL_PENCIL = 3901,
    IDC_ICON_TOOL_ERASER,
    IDC_ICON_TOOL_FILL,
    IDC_ICON_TOOL_EYEDROPPER,
    IDC_ICON_TOOL_CLEAR,
    IDC_ICON_GRID_CHK,
    IDC_ICON_IMPORT_BTN,
    IDC_ICON_PASTE_BTN,
    IDC_ICON_EXP_PNG,
    IDC_ICON_EXP_DDS,
    IDC_ICON_ASSIGN_BTN,
    IDC_ICON_RANK_COMBO,
    IDC_ICON_APPLY_BORDER,
    IDC_ICON_PALETTE_BASE = 3950,
};

} // namespace

BEGIN_MESSAGE_MAP(CThemedDialog, CDialog)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CThemedDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    if (HBRUSH hbr = Theme::CtlColor(pDC, pWnd, nCtlColor)) return hbr;
    return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

BEGIN_MESSAGE_MAP(CIconPickerDlg, CThemedDialog)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MOUSEMOVE()
    ON_BN_CLICKED(IDC_ICON_PREV, &CIconPickerDlg::OnPrevPage)
    ON_BN_CLICKED(IDC_ICON_NEXT, &CIconPickerDlg::OnNextPage)
    ON_CBN_SELCHANGE(IDC_ICON_PAGE_COMBO, &CIconPickerDlg::OnPageComboChanged)
    ON_BN_CLICKED(IDC_ICON_JUMP_BTN, &CIconPickerDlg::OnJumpToIndex)
END_MESSAGE_MAP()

CIconPickerDlg::CIconPickerDlg(Engine::WorkspaceEngine& engine, int32_t initialIconIndex, CWnd* parent)
    : m_tmpl(L"Dragon Nest Icon Atlas Picker", 380, 440),
      m_engine(engine),
      m_selectedIndex(std::max(0, initialIconIndex)) {
    m_currentPage = m_selectedIndex / 200 + 1;
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CIconPickerDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    const int pad = DpiScale(12, dpi);
    const int cellPx = DpiScale(34, dpi);
    const int gridW = cellPx * 10;
    const int gridH = cellPx * 20;
    const int sideW = DpiScale(216, dpi);
    const int topBarH = DpiScale(32, dpi);

    const int clientW = pad + gridW + pad + sideW + pad;
    const int clientH = pad + topBarH + DpiScale(8, dpi) + gridH + pad;

    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    int x = pad;
    const int btnH = DpiScale(26, dpi);
    m_btnPrev.Create(L"< Prev", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                     CRect(x, pad, x + DpiScale(68, dpi), pad + btnH), this, IDC_ICON_PREV);
    x += DpiScale(74, dpi);

    m_cmbPage.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                     CRect(x, pad + DpiScale(1, dpi), x + DpiScale(175, dpi), pad + DpiScale(320, dpi)),
                     this, IDC_ICON_PAGE_COMBO);
    x += DpiScale(181, dpi);

    m_btnNext.Create(L"Next >", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                     CRect(x, pad, x + DpiScale(68, dpi), pad + btnH), this, IDC_ICON_NEXT);
    x += DpiScale(80, dpi);

    m_edtDirectIndex.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                            CRect(x, pad + DpiScale(1, dpi), x + DpiScale(88, dpi), pad + btnH),
                            this, IDC_ICON_DIRECT_EDIT);
    x += DpiScale(94, dpi);

    m_btnJump.Create(L"Go #ID", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                     CRect(x, pad, x + DpiScale(68, dpi), pad + btnH), this, IDC_ICON_JUMP_BTN);

    const int maxPage = std::max(1, m_engine.GetMaxIconAtlasPage());
    for (int p = 1; p <= maxPage; ++p) {
        const std::string label = std::format("Page {:02d}  (itemicon{:02d}.dds)", p, p);
        m_cmbPage.AddString(Utf8ToWide(label));
    }

    const int gridTop = pad + topBarH + DpiScale(8, dpi);
    m_gridRect = CRect(pad, gridTop, pad + gridW, gridTop + gridH);

    const int sideLeft = m_gridRect.right + pad;
    const int zoomSz = DpiScale(100, dpi);
    m_zoomRect = CRect(sideLeft, gridTop, sideLeft + zoomSz, gridTop + zoomSz);

    m_lblInfo.Create(L"", WS_CHILD | WS_VISIBLE,
                     CRect(sideLeft, m_zoomRect.bottom + DpiScale(12, dpi),
                           sideLeft + sideW, m_gridRect.bottom - DpiScale(76, dpi)),
                     this, IDC_ICON_INFO_LBL);

    m_btnOk.Create(L"Select Icon", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                   CRect(sideLeft, m_gridRect.bottom - DpiScale(64, dpi),
                         sideLeft + sideW, m_gridRect.bottom - DpiScale(34, dpi)),
                   this, IDOK);
    m_btnCancel.Create(L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(sideLeft, m_gridRect.bottom - DpiScale(28, dpi),
                             sideLeft + sideW, m_gridRect.bottom),
                       this, IDCANCEL);

    for (CWnd* w : { static_cast<CWnd*>(&m_btnPrev), static_cast<CWnd*>(&m_cmbPage), static_cast<CWnd*>(&m_btnNext),
                     static_cast<CWnd*>(&m_edtDirectIndex), static_cast<CWnd*>(&m_btnJump),
                     static_cast<CWnd*>(&m_lblInfo), static_cast<CWnd*>(&m_btnOk), static_cast<CWnd*>(&m_btnCancel) }) {
        w->SetFont(&m_fonts.regular);
    }

    ApplyFlatChildStyles(this);
    m_edtDirectIndex.SetWindowTextW(Utf8ToWide(std::to_string(m_selectedIndex)));
    LoadPage(std::clamp(m_currentPage, 1, maxPage));
    return TRUE;
}

void CIconPickerDlg::LoadPage(int page) {
    const int maxPage = std::max(1, m_engine.GetMaxIconAtlasPage());
    m_currentPage = std::clamp(page, 1, maxPage);
    m_cmbPage.SetCurSel(m_currentPage - 1);

    m_atlas = m_engine.GetIconAtlasPage(m_currentPage);
    m_croppedAtlasGrid = {};
    if (m_atlas && m_atlas->width >= 500 && m_atlas->height >= 1000) {
        m_croppedAtlasGrid.width = 500;
        m_croppedAtlasGrid.height = 1000;
        m_croppedAtlasGrid.rgbaData.resize(500 * 1000 * 4);
        for (uint32_t y = 0; y < 1000; ++y) {
            std::memcpy(&m_croppedAtlasGrid.rgbaData[static_cast<size_t>(y) * 500 * 4],
                        &m_atlas->rgbaData[static_cast<size_t>(y) * m_atlas->width * 4],
                        500 * 4);
        }
    }

    m_hasSelectedTileImg = m_engine.GetItemIcon(m_selectedIndex, m_selectedTileImg);
    UpdateInfoText();
    Invalidate(FALSE);
}

void CIconPickerDlg::UpdateInfoText() {
    const int selPage = m_selectedIndex / 200 + 1;
    const int selTile = m_selectedIndex % 200;
    std::string text = std::format(
        "Selected Icon: #{}\r\n"
        "Atlas: itemicon{:02d}.dds\r\n"
        "Tile: {} (Col {}, Row {})\r\n\r\n",
        m_selectedIndex, selPage, selTile, selTile % 10, selTile / 10);

    if (m_hoverTile >= 0 && m_hoverTile < 200) {
        const int32_t hoverIdx = (m_currentPage - 1) * 200 + m_hoverTile;
        text += std::format("Hovering: #{}\r\n(Col {}, Row {})\r\n\r\n",
                            hoverIdx, m_hoverTile % 10, m_hoverTile / 10);
    }
    text += "Tip: Click any icon to select, or double-click an icon to apply immediately.";
    m_lblInfo.SetWindowTextW(Utf8ToWide(text));
}

int CIconPickerDlg::TileFromPoint(CPoint pt) const {
    if (!m_gridRect.PtInRect(pt) || m_gridRect.Width() <= 0 || m_gridRect.Height() <= 0) return -1;
    const int col = std::clamp<int>(static_cast<int>((pt.x - m_gridRect.left) * 10 / m_gridRect.Width()), 0, 9);
    const int row = std::clamp<int>(static_cast<int>((pt.y - m_gridRect.top) * 20 / m_gridRect.Height()), 0, 19);
    return row * 10 + col;
}

BOOL CIconPickerDlg::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE;
}

void CIconPickerDlg::OnPaint() {
    CPaintDC paintDC(this);
    CRect client;
    GetClientRect(&client);
    if (client.Width() <= 0 || client.Height() <= 0) return;

    CDC memDC;
    memDC.CreateCompatibleDC(&paintDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paintDC, client.Width(), client.Height());
    CBitmap* oldBmp = memDC.SelectObject(&bmp);

    const COLORREF bg = Theme::kChrome;
    memDC.FillSolidRect(&client, bg);

    const COLORREF slotBg = RGB(20, 22, 28);
    memDC.FillSolidRect(&m_gridRect, slotBg);
    if (m_croppedAtlasGrid.width > 0) {
        BlitImageBuffer(memDC.GetSafeHdc(), m_gridRect, m_croppedAtlasGrid, slotBg, true);
    }

    CPen gridPen(PS_SOLID, 1, RGB(46, 52, 66));
    CPen* oldPen = memDC.SelectObject(&gridPen);
    for (int c = 1; c < 10; ++c) {
        const int x = m_gridRect.left + m_gridRect.Width() * c / 10;
        memDC.MoveTo(x, m_gridRect.top);
        memDC.LineTo(x, m_gridRect.bottom);
    }
    for (int r = 1; r < 20; ++r) {
        const int y = m_gridRect.top + m_gridRect.Height() * r / 20;
        memDC.MoveTo(m_gridRect.left, y);
        memDC.LineTo(m_gridRect.right, y);
    }
    memDC.SelectObject(oldPen);
    Theme::FrameRect1px(memDC, m_gridRect, Theme::kBorder);

    auto cellRect = [&](int tile) {
        const int c = tile % 10;
        const int r = tile / 10;
        const int x0 = m_gridRect.left + m_gridRect.Width() * c / 10;
        const int x1 = m_gridRect.left + m_gridRect.Width() * (c + 1) / 10;
        const int y0 = m_gridRect.top + m_gridRect.Height() * r / 20;
        const int y1 = m_gridRect.top + m_gridRect.Height() * (r + 1) / 20;
        return CRect(x0, y0, x1, y1);
    };

    if (m_hoverTile >= 0 && m_hoverTile < 200) {
        CRect hr = cellRect(m_hoverTile);
        memDC.Draw3dRect(&hr, RGB(120, 190, 255), RGB(120, 190, 255));
    }

    if (m_selectedIndex / 200 + 1 == m_currentPage) {
        CRect sr = cellRect(m_selectedIndex % 200);
        memDC.Draw3dRect(&sr, RGB(255, 205, 65), RGB(255, 205, 65));
        sr.DeflateRect(1, 1);
        memDC.Draw3dRect(&sr, RGB(255, 205, 65), RGB(255, 205, 65));
    }

    memDC.FillSolidRect(&m_zoomRect, slotBg);
    if (m_hasSelectedTileImg) {
        CRect inner = m_zoomRect;
        inner.DeflateRect(2, 2);
        BlitImageBuffer(memDC.GetSafeHdc(), inner, m_selectedTileImg, slotBg, false);
    }
    Theme::FrameRect1px(memDC, m_zoomRect, Theme::kBorder);

    paintDC.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(oldBmp);
}

void CIconPickerDlg::OnLButtonDown(UINT nFlags, CPoint point) {
    const int tile = TileFromPoint(point);
    if (tile >= 0) {
        m_selectedIndex = (m_currentPage - 1) * 200 + tile;
        m_edtDirectIndex.SetWindowTextW(Utf8ToWide(std::to_string(m_selectedIndex)));
        m_hasSelectedTileImg = m_engine.GetItemIcon(m_selectedIndex, m_selectedTileImg);
        UpdateInfoText();
        Invalidate(FALSE);
    }
    CDialog::OnLButtonDown(nFlags, point);
}

void CIconPickerDlg::OnLButtonDblClk(UINT nFlags, CPoint point) {
    const int tile = TileFromPoint(point);
    if (tile >= 0) {
        m_selectedIndex = (m_currentPage - 1) * 200 + tile;
        EndDialog(IDOK);
        return;
    }
    CDialog::OnLButtonDblClk(nFlags, point);
}

void CIconPickerDlg::OnMouseMove(UINT nFlags, CPoint point) {
    const int tile = TileFromPoint(point);
    if (tile != m_hoverTile) {
        m_hoverTile = tile;
        UpdateInfoText();
        InvalidateRect(&m_gridRect, FALSE);
    }
    CDialog::OnMouseMove(nFlags, point);
}

void CIconPickerDlg::OnPrevPage() {
    if (m_currentPage > 1) LoadPage(m_currentPage - 1);
}

void CIconPickerDlg::OnNextPage() {
    if (m_currentPage < m_engine.GetMaxIconAtlasPage()) LoadPage(m_currentPage + 1);
}

void CIconPickerDlg::OnPageComboChanged() {
    const int sel = m_cmbPage.GetCurSel();
    if (sel >= 0) LoadPage(sel + 1);
}

void CIconPickerDlg::OnJumpToIndex() {
    CString txt;
    m_edtDirectIndex.GetWindowTextW(txt);
    try {
        const int32_t idx = std::max(0, std::stoi(WideToUtf8(txt)));
        m_selectedIndex = idx;
        LoadPage(idx / 200 + 1);
    } catch (...) {
    }
}

void CIconPickerDlg::OnOK() {
    CDialog::OnOK();
}

BEGIN_MESSAGE_MAP(CNewItemDlg, CThemedDialog)
    ON_CBN_SELCHANGE(IDC_NEW_TABLE_COMBO, &CNewItemDlg::OnTableChanged)
    ON_CBN_SELCHANGE(IDC_NEW_TYPE_COMBO, &CNewItemDlg::OnTypeChanged)
    ON_BN_CLICKED(IDC_NEW_ID_SUGGEST, &CNewItemDlg::OnSuggestId)
END_MESSAGE_MAP()

CNewItemDlg::CNewItemDlg(const Engine::WorkspaceEngine& engine,
                         bool duplicateMode,
                         int defaultTableIndex,
                         int32_t sourceItemId,
                         const std::string& defaultName,
                         CWnd* parent)
    : m_tmpl(duplicateMode ? L"Duplicate Dragon Nest Item" : L"Create New Dragon Nest Item", 320, 210),
      m_engine(engine),
      m_duplicateMode(duplicateMode),
      m_defaultTableIndex(std::max(0, defaultTableIndex)),
      m_sourceItemId(sourceItemId),
      m_defaultName(defaultName) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CNewItemDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    const int pad = DpiScale(14, dpi);
    const int lblW = DpiScale(130, dpi);
    const int fldW = DpiScale(320, dpi);
    const int rowH = DpiScale(26, dpi);
    const int gapY = DpiScale(10, dpi);

    const int clientW = pad + lblW + DpiScale(10, dpi) + fldW + pad;
    const int clientH = pad + (rowH + gapY) * 6 + DpiScale(20, dpi) + pad;

    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    int y = pad;
    const int xLbl = pad;
    const int xFld = pad + lblW + DpiScale(10, dpi);

    const std::string hdrText = m_duplicateMode
                                    ? std::format("Clone item #{} into a fresh row ID with dedicated exclusive UIString MIDs:", m_sourceItemId)
                                    : "Create a brand-new item row (and joined weapon/parts detail row if equipment):";
    m_lblHeader.Create(Utf8ToWide(hdrText), WS_CHILD | WS_VISIBLE,
                       CRect(xLbl, y, clientW - pad, y + rowH), this);
    y += rowH + gapY;

    m_lblName.Create(L"Item Name:", WS_CHILD | WS_VISIBLE, CRect(xLbl, y + DpiScale(3, dpi), xLbl + lblW, y + rowH), this);
    m_edtName.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                     CRect(xFld, y, xFld + fldW, y + rowH), this, IDC_NEW_NAME_EDIT);
    m_edtName.SetWindowTextW(Utf8ToWide(m_defaultName.empty() ? "Custom Dragon Item" : m_defaultName));
    y += rowH + gapY;

    m_lblTable.Create(L"Target Item Table:", WS_CHILD | WS_VISIBLE, CRect(xLbl, y + DpiScale(3, dpi), xLbl + lblW, y + rowH), this);
    m_cmbTable.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                      CRect(xFld, y, xFld + fldW, y + DpiScale(280, dpi)), this, IDC_NEW_TABLE_COMBO);
    const auto& im = m_engine.Items();
    for (size_t i = 0; i < im.GetItemTableCount(); ++i) {
        const auto& tbl = im.GetItemTable(i);
        const std::string entry = std::format("{}  ({} rows)", tbl.GetFileName(), tbl.GetRowCount());
        m_cmbTable.AddString(Utf8ToWide(entry));
    }
    if (im.GetItemTableCount() > 0) {
        m_cmbTable.SetCurSel(std::clamp(m_defaultTableIndex, 0, static_cast<int>(im.GetItemTableCount()) - 1));
    }
    y += rowH + gapY;

    m_lblId.Create(L"New Item Row ID:", WS_CHILD | WS_VISIBLE, CRect(xLbl, y + DpiScale(3, dpi), xLbl + lblW, y + rowH), this);
    const int sugBtnW = DpiScale(104, dpi);
    m_edtId.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                   CRect(xFld, y, xFld + fldW - sugBtnW - DpiScale(6, dpi), y + rowH), this, IDC_NEW_ID_EDIT);
    m_btnSuggestId.Create(L"Auto Next ID", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                         CRect(xFld + fldW - sugBtnW, y, xFld + fldW, y + rowH),
                         this, IDC_NEW_ID_SUGGEST);
    y += rowH + gapY;

    m_lblType.Create(L"Item Type (_Type):", WS_CHILD | WS_VISIBLE, CRect(xLbl, y + DpiScale(3, dpi), xLbl + lblW, y + rowH), this);
    m_cmbType.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                     CRect(xFld, y, xFld + fldW, y + DpiScale(260, dpi)), this, IDC_NEW_TYPE_COMBO);
    int defaultTypeSel = 0;
    const auto& knownTypes = Data::ItemManager::KnownItemTypes();
    const Data::DNItem* srcItem = m_duplicateMode ? im.GetItem(m_sourceItemId) : nullptr;
    for (size_t i = 0; i < knownTypes.size(); ++i) {
        const std::string tLabel = std::format("{} - {}", knownTypes[i].first, knownTypes[i].second);
        const int idx = m_cmbType.AddString(Utf8ToWide(tLabel));
        m_cmbType.SetItemData(idx, static_cast<DWORD_PTR>(knownTypes[i].first));
        if (srcItem && srcItem->rawType == knownTypes[i].first) defaultTypeSel = idx;
    }
    m_cmbType.SetCurSel(defaultTypeSel);
    if (m_duplicateMode) m_cmbType.EnableWindow(FALSE);
    y += rowH + gapY;

    m_lblSubType.Create(L"Equipment Slot:", WS_CHILD | WS_VISIBLE, CRect(xLbl, y + DpiScale(3, dpi), xLbl + lblW, y + rowH), this);
    m_cmbSubType.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                        CRect(xFld, y, xFld + fldW, y + DpiScale(260, dpi)), this, IDC_NEW_SUBTYPE_COMBO);
    PopulateSubTypes(srcItem ? srcItem->rawType : static_cast<int32_t>(m_cmbType.GetItemData(defaultTypeSel)));
    if (m_duplicateMode) m_cmbSubType.EnableWindow(FALSE);
    y += rowH + gapY + DpiScale(6, dpi);

    const int btnW = DpiScale(124, dpi);
    m_btnOk.Create(m_duplicateMode ? L"Duplicate Item" : L"Create Item",
                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                   CRect(clientW - pad - btnW * 2 - DpiScale(8, dpi), y, clientW - pad - btnW - DpiScale(8, dpi), y + DpiScale(30, dpi)),
                   this, IDOK);
    m_btnCancel.Create(L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(clientW - pad - btnW, y, clientW - pad, y + DpiScale(30, dpi)),
                       this, IDCANCEL);

    for (CWnd* w : { static_cast<CWnd*>(&m_lblHeader), static_cast<CWnd*>(&m_lblName), static_cast<CWnd*>(&m_edtName),
                     static_cast<CWnd*>(&m_lblTable), static_cast<CWnd*>(&m_cmbTable), static_cast<CWnd*>(&m_lblId),
                     static_cast<CWnd*>(&m_edtId), static_cast<CWnd*>(&m_btnSuggestId), static_cast<CWnd*>(&m_lblType),
                     static_cast<CWnd*>(&m_cmbType), static_cast<CWnd*>(&m_lblSubType), static_cast<CWnd*>(&m_cmbSubType),
                     static_cast<CWnd*>(&m_btnOk), static_cast<CWnd*>(&m_btnCancel) }) {
        w->SetFont(&m_fonts.regular);
    }
    m_lblHeader.SetFont(&m_fonts.bold);

    ApplyFlatChildStyles(this);
    OnSuggestId();
    return TRUE;
}

void CNewItemDlg::PopulateSubTypes(int32_t rawType) {
    m_cmbSubType.ResetContent();
    if (rawType == 0) {
        static const char* kWeapons[] = {
            "0 - Sword", "1 - Gauntlet", "2 - Axe", "3 - Hammer", "4 - Short Bow", "5 - Long Bow",
            "6 - Crossbow", "7 - Staff", "8 - Book", "9 - Orb", "10 - Puppet", "11 - Mace",
            "12 - Flail", "13 - Wand", "14 - Shield", "15 - Arrow", "16 - Cannon", "17 - Bubble Gun",
            "18 - Chain", "19 - Fan", "20 - Chakram", "21 - Charm", "22 - Scimitar", "23 - Dagger",
            "24 - Sickle", "25 - Spear", "27 - Bracelet"
        };
        static const int kWeaponIds[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27 };
        for (size_t i = 0; i < std::size(kWeapons); ++i) {
            const int idx = m_cmbSubType.AddString(Utf8ToWide(kWeapons[i]));
            m_cmbSubType.SetItemData(idx, static_cast<DWORD_PTR>(kWeaponIds[i]));
        }
        m_cmbSubType.SetCurSel(0);
        m_cmbSubType.EnableWindow(!m_duplicateMode);
    } else if (rawType == 1) {
        static const std::pair<int, const char*> kParts[] = {
            { 2, "2 - Helmet" }, { 3, "3 - Body / Upper" }, { 4, "4 - Legs / Lower" },
            { 5, "5 - Gloves" }, { 6, "6 - Boots" }, { 7, "7 - Necklace" },
            { 8, "8 - Earrings" }, { 9, "9 - Ring" }
        };
        for (const auto& p : kParts) {
            const int idx = m_cmbSubType.AddString(Utf8ToWide(p.second));
            m_cmbSubType.SetItemData(idx, static_cast<DWORD_PTR>(p.first));
        }
        m_cmbSubType.SetCurSel(1);
        m_cmbSubType.EnableWindow(!m_duplicateMode);
    } else {
        const int idx = m_cmbSubType.AddString(L"(Not applicable for non-equipment)");
        m_cmbSubType.SetItemData(idx, static_cast<DWORD_PTR>(-1));
        m_cmbSubType.SetCurSel(0);
        m_cmbSubType.EnableWindow(FALSE);
    }
}

void CNewItemDlg::OnTableChanged() {
    OnSuggestId();
}

void CNewItemDlg::OnTypeChanged() {
    const int sel = m_cmbType.GetCurSel();
    if (sel >= 0) {
        PopulateSubTypes(static_cast<int32_t>(m_cmbType.GetItemData(sel)));
    }
}

void CNewItemDlg::OnSuggestId() {
    const int tblSel = std::max(0, m_cmbTable.GetCurSel());
    const int32_t nearId = (m_duplicateMode && tblSel == m_defaultTableIndex) ? m_sourceItemId : 0;
    const int32_t suggested = m_engine.Items().SuggestNextItemId(tblSel, nearId);
    m_edtId.SetWindowTextW(Utf8ToWide(std::to_string(suggested)));
}

void CNewItemDlg::OnOK() {
    CString wName, wId;
    m_edtName.GetWindowTextW(wName);
    m_edtId.GetWindowTextW(wId);

    m_resultName = WideToUtf8(wName);
    m_resultTableIndex = std::max(0, m_cmbTable.GetCurSel());

    int32_t parsedId = 0;
    try {
        parsedId = std::stoi(WideToUtf8(wId));
    } catch (...) {
        parsedId = 0;
    }
    if (parsedId <= 0) {
        MessageBoxW(L"Please enter a positive Item Row ID.", L"Invalid Row ID", MB_ICONWARNING | MB_OK);
        return;
    }
    if (m_engine.Items().GetItem(parsedId) != nullptr) {
        MessageBoxW(L"That Item Row ID already exists in the loaded item tables. Click 'Auto Next ID' or enter a unique ID.",
                    L"Duplicate Row ID", MB_ICONWARNING | MB_OK);
        return;
    }
    m_resultItemId = parsedId;

    const int typeSel = m_cmbType.GetCurSel();
    m_resultRawType = typeSel >= 0 ? static_cast<int32_t>(m_cmbType.GetItemData(typeSel)) : 2;

    const int subSel = m_cmbSubType.GetCurSel();
    m_resultEquipSubType = subSel >= 0 ? static_cast<int32_t>(m_cmbSubType.GetItemData(subSel)) : -1;

    CDialog::OnOK();
}

BEGIN_MESSAGE_MAP(CUIStringSearchDlg, CThemedDialog)
    ON_EN_CHANGE(IDC_UISTR_QUERY, &CUIStringSearchDlg::OnQueryChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_UISTR_LIST, &CUIStringSearchDlg::OnListDblClick)
END_MESSAGE_MAP()

CUIStringSearchDlg::CUIStringSearchDlg(const Engine::WorkspaceEngine& engine,
                                       std::string initialQuery,
                                       CWnd* parent)
    : m_tmpl(L"Global UIString.xml Lookup & MID Binder", 420, 290),
      m_engine(engine),
      m_initialQuery(std::move(initialQuery)) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CUIStringSearchDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    const int pad = DpiScale(14, dpi);
    const int clientW = DpiScale(680, dpi);
    const int clientH = DpiScale(460, dpi);

    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_lblSearch.Create(L"Search text or MID:", WS_CHILD | WS_VISIBLE,
                       CRect(pad, pad + DpiScale(4, dpi), pad + DpiScale(135, dpi), pad + DpiScale(26, dpi)), this);
    m_edtQuery.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                      CRect(pad + DpiScale(140, dpi), pad, clientW - pad - DpiScale(150, dpi), pad + DpiScale(26, dpi)),
                      this, IDC_UISTR_QUERY);
    m_lblCount.Create(L"", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                      CRect(clientW - pad - DpiScale(142, dpi), pad + DpiScale(4, dpi), clientW - pad, pad + DpiScale(26, dpi)),
                      this, IDC_UISTR_COUNT);

    const int btnH = DpiScale(30, dpi);
    const int listTop = pad + DpiScale(34, dpi);
    const int listBottom = clientH - pad - btnH - DpiScale(10, dpi);

    m_lstHits.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                     CRect(pad, listTop, clientW - pad, listBottom), this, IDC_UISTR_LIST);
    m_lstHits.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    m_lstHits.InsertColumn(0, L"MID", LVCFMT_RIGHT, DpiScale(90, dpi));
    m_lstHits.InsertColumn(1, L"Used By", LVCFMT_RIGHT, DpiScale(75, dpi));
    m_lstHits.InsertColumn(2, L"UIString Text", LVCFMT_LEFT, clientW - pad * 2 - DpiScale(190, dpi));

    m_btnOk.Create(L"Bind Selected MID", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                   CRect(clientW - pad - DpiScale(258, dpi), clientH - pad - btnH,
                         clientW - pad - DpiScale(114, dpi), clientH - pad),
                   this, IDOK);
    m_btnCancel.Create(L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(clientW - pad - DpiScale(106, dpi), clientH - pad - btnH,
                             clientW - pad, clientH - pad),
                       this, IDCANCEL);

    for (CWnd* w : { static_cast<CWnd*>(&m_lblSearch), static_cast<CWnd*>(&m_edtQuery),
                     static_cast<CWnd*>(&m_lblCount), static_cast<CWnd*>(&m_lstHits),
                     static_cast<CWnd*>(&m_btnOk), static_cast<CWnd*>(&m_btnCancel) }) {
        w->SetFont(&m_fonts.regular);
    }

    m_edtQuery.SetWindowTextW(Utf8ToWide(m_initialQuery));
    RunSearch();
    m_edtQuery.SetFocus();
    return FALSE;
}

void CUIStringSearchDlg::OnQueryChanged() {
    RunSearch();
}

void CUIStringSearchDlg::RunSearch() {
    CString wq;
    m_edtQuery.GetWindowTextW(wq);
    const std::string q = WideToUtf8(wq);

    m_hits = m_engine.Items().GetUIStrings().SearchStrings(q, 300);

    m_lstHits.SetRedraw(FALSE);
    m_lstHits.DeleteAllItems();
    for (size_t i = 0; i < m_hits.size(); ++i) {
        const auto& h = m_hits[i];
        const int row = m_lstHits.InsertItem(static_cast<int>(i), Utf8ToWide(std::to_string(h.mid)));
        const size_t refs = m_engine.Items().CountItemsUsingMid(h.mid);
        m_lstHits.SetItemText(row, 1, Utf8ToWide(std::format("{} item{}", refs, refs == 1 ? "" : "s")));
        m_lstHits.SetItemText(row, 2, Utf8ToWide(h.text));
    }
    if (!m_hits.empty()) {
        m_lstHits.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    m_lstHits.SetRedraw(TRUE);

    const std::string cnt = std::format("{}{} matches",
                                        m_hits.size(),
                                        m_hits.size() >= 300 ? "+" : "");
    m_lblCount.SetWindowTextW(Utf8ToWide(cnt));
}

void CUIStringSearchDlg::OnListDblClick(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    *pResult = 0;
    OnOK();
}

void CUIStringSearchDlg::OnOK() {
    const int sel = m_lstHits.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || static_cast<size_t>(sel) >= m_hits.size()) {
        MessageBoxW(L"Select a UIString row to bind.", L"UIString Lookup", MB_ICONINFORMATION | MB_OK);
        return;
    }
    m_selectedMid = m_hits[static_cast<size_t>(sel)].mid;
    m_selectedText = m_hits[static_cast<size_t>(sel)].text;
    CDialog::OnOK();
}

BEGIN_MESSAGE_MAP(CReportDlg, CThemedDialog)
    ON_BN_CLICKED(IDC_REPORT_COPY, &CReportDlg::OnCopyClipboard)
    ON_BN_CLICKED(IDC_REPORT_SAVE, &CReportDlg::OnSaveToFile)
END_MESSAGE_MAP()

CReportDlg::CReportDlg(std::string title, std::string summaryHeader, std::string bodyText, CWnd* parent)
    : m_tmpl(std::wstring(Utf8ToWide(title).GetString()), 460, 310),
      m_summaryHeader(std::move(summaryHeader)),
      m_bodyText(std::move(bodyText)) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CReportDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    const int pad = DpiScale(14, dpi);
    const int clientW = DpiScale(760, dpi);
    const int clientH = DpiScale(520, dpi);

    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_lblHeader.Create(Utf8ToWide(m_summaryHeader), WS_CHILD | WS_VISIBLE,
                       CRect(pad, pad, clientW - pad, pad + DpiScale(24, dpi)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    const int btnH = DpiScale(30, dpi);
    const int btnW = DpiScale(144, dpi);
    const int editTop = pad + DpiScale(30, dpi);
    const int editBottom = clientH - pad - btnH - DpiScale(10, dpi);

    m_edtReport.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
                           ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                       CRect(pad, editTop, clientW - pad, editBottom), this, IDC_REPORT_EDIT);
    m_edtReport.SetFont(&m_fonts.mono);
    m_edtReport.SetLimitText(0);
    m_edtReport.SetWindowTextW(Utf8ToWide(m_bodyText));

    m_btnCopy.Create(L"Copy to Clipboard", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                     CRect(pad, clientH - pad - btnH, pad + btnW, clientH - pad),
                     this, IDC_REPORT_COPY);
    m_btnCopy.SetFont(&m_fonts.regular);

    m_btnSave.Create(L"Save to File...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                     CRect(pad + btnW + DpiScale(8, dpi), clientH - pad - btnH,
                           pad + btnW * 2 + DpiScale(8, dpi), clientH - pad),
                     this, IDC_REPORT_SAVE);
    m_btnSave.SetFont(&m_fonts.regular);

    m_btnClose.Create(L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                      CRect(clientW - pad - DpiScale(110, dpi), clientH - pad - btnH, clientW - pad, clientH - pad),
                      this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    return TRUE;
}

void CReportDlg::OnCopyClipboard() {
    if (!OpenClipboard()) return;
    ::EmptyClipboard();
    const CString wide = Utf8ToWide(m_bodyText);
    const size_t bytes = static_cast<size_t>(wide.GetLength() + 1) * sizeof(wchar_t);
    if (HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* ptr = ::GlobalLock(hMem)) {
            std::memcpy(ptr, wide.GetString(), bytes);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    ::CloseClipboard();
}

void CReportDlg::OnSaveToFile() {
    CFileDialog dlg(FALSE, L"txt", L"DNItemMaker_Report.txt",
                    OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    L"Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        std::ofstream out(std::filesystem::path(dlg.GetPathName().GetString()), std::ios::binary);
        if (out.is_open()) {
            out.write(m_bodyText.data(), static_cast<std::streamsize>(m_bodyText.size()));
        }
    }
}

CAboutDlg::CAboutDlg(CWnd* parent)
    : m_tmpl(L"About Dragon Nest Item Maker", 260, 150) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CAboutDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);

    const int pad = DpiScale(16, dpi);
    const int clientW = DpiScale(400, dpi);
    const int clientH = DpiScale(220, dpi);

    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    const int iconSz = DpiScale(32, dpi);
    m_lblIcon.Create(L"", WS_CHILD | WS_VISIBLE | SS_ICON,
                     CRect(pad, pad, pad + iconSz, pad + iconSz), this);
    m_lblIcon.SetIcon(::LoadIconW(nullptr, IDI_INFORMATION));

    const int textX = pad + iconSz + DpiScale(14, dpi);
    const int textW = clientW - textX - pad;
    int y = pad;

    m_lblTitle.Create(L"Dragon Nest Item Maker", WS_CHILD | WS_VISIBLE,
                      CRect(textX, y, textX + textW, y + DpiScale(20, dpi)), this);
    m_lblTitle.SetFont(&m_fonts.title);
    y += DpiScale(24, dpi);

    m_lblVersion.Create(Utf8ToWide(std::format("Version {} (Release)", VERSION_STRING)),
                        WS_CHILD | WS_VISIBLE,
                        CRect(textX, y, textX + textW, y + DpiScale(18, dpi)), this);
    m_lblVersion.SetFont(&m_fonts.regular);
    y += DpiScale(20, dpi);

    m_lblBuildDate.Create(Utf8ToWide(std::format("Build Date: {} {}", __DATE__, __TIME__)),
                          WS_CHILD | WS_VISIBLE,
                          CRect(textX, y, textX + textW, y + DpiScale(18, dpi)), this);
    m_lblBuildDate.SetFont(&m_fonts.regular);
    y += DpiScale(20, dpi);

    m_lblArch.Create(L"Target Platform: Windows x64 (MSVC C++23 Native MFC)",
                     WS_CHILD | WS_VISIBLE,
                     CRect(textX, y, textX + textW, y + DpiScale(18, dpi)), this);
    m_lblArch.SetFont(&m_fonts.regular);
    y += DpiScale(20, dpi);

    m_lblCopyright.Create(L"Copyright \x00a9 2026. All rights reserved.",
                          WS_CHILD | WS_VISIBLE,
                          CRect(textX, y, textX + textW, y + DpiScale(18, dpi)), this);
    m_lblCopyright.SetFont(&m_fonts.regular);

    const int btnW = DpiScale(80, dpi);
    const int btnH = DpiScale(26, dpi);
    m_btnOk.Create(L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                   CRect(clientW - pad - btnW, clientH - pad - btnH, clientW - pad, clientH - pad),
                   this, IDOK);
    m_btnOk.SetFont(&m_fonts.regular);

    return TRUE;
}


// ============================================================================
// CDragonJewelStudioDlg
// ============================================================================

BEGIN_MESSAGE_MAP(CDragonJewelStudioDlg, CThemedDialog)
    ON_CBN_SELCHANGE(IDC_DJ_TYPE_COMBO, &CDragonJewelStudioDlg::OnTypeFilterChanged)
    ON_CBN_SELCHANGE(IDC_DJ_RANK_COMBO, &CDragonJewelStudioDlg::OnRankFilterChanged)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_DJ_JEWEL_LIST, &CDragonJewelStudioDlg::OnJewelItemSelect)
    ON_BN_CLICKED(IDC_DJ_APPLY_CFG, &CDragonJewelStudioDlg::OnApplySocketConfig)
    ON_BN_CLICKED(IDC_DJ_SOCKET_1, &CDragonJewelStudioDlg::OnSocketSlot1)
    ON_BN_CLICKED(IDC_DJ_SOCKET_2, &CDragonJewelStudioDlg::OnSocketSlot2)
    ON_BN_CLICKED(IDC_DJ_SOCKET_3, &CDragonJewelStudioDlg::OnSocketSlot3)
    ON_BN_CLICKED(IDC_DJ_SOCKET_4, &CDragonJewelStudioDlg::OnSocketSlot4)
    ON_BN_CLICKED(IDC_DJ_CLEAR_ALL, &CDragonJewelStudioDlg::OnClearAllSockets)
    ON_NOTIFY(NM_DBLCLK, IDC_DJ_EQUIP_LIST, &CDragonJewelStudioDlg::OnEquipItemSelect)
END_MESSAGE_MAP()

CDragonJewelStudioDlg::CDragonJewelStudioDlg(Engine::WorkspaceEngine& engine, int32_t currentItemId, CWnd* parent)
    : m_tmpl(L"Dragon Jewel & Socket Studio", 580, 420, WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
      m_engine(engine),
      m_selectedItemId(currentItemId) {
    InitModalIndirect(m_tmpl.Get(), parent);
    const auto* it = m_engine.Items().GetItem(m_selectedItemId);
    if (it) {
        m_currentConfigId = it->dragonJewelType;
        m_isJewelItem = m_engine.Items().IsDragonJewel(*it);
    }
}

BOOL CDragonJewelStudioDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);

    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int clientW = dluX(580);
    const int clientH = dluY(420);
    CRect curClient, curWin;
    GetClientRect(&curClient);
    GetWindowRect(&curWin);
    const int extraW = curWin.Width() - curClient.Width();
    const int extraH = curWin.Height() - curClient.Height();
    SetWindowPos(nullptr, 0, 0, clientW + extraW, clientH + extraH, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    const auto* curItem = m_engine.Items().GetItem(m_selectedItemId);
    std::wstring hdr = L"Target Item: (None selected)";
    if (curItem) {
        hdr = std::format(L"Target Item: #{} '{}' (Rank: {}, Lv: {})",
                          curItem->id, Utf8ToWide(curItem->name).GetString(),
                          Utf8ToWide(Data::ItemManager::RankName(curItem->rank)).GetString(),
                          curItem->levelLimit);
    }
    m_lblHeader.Create(hdr.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                       CRect(dluX(10), dluY(8), dluX(510), dluY(20)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    // Left pane: Socket configuration & Jade socketing
    m_grpSockets.Create(L"Dragon Jewel Sockets", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        CRect(dluX(10), dluY(24), dluX(220), dluY(250)), this);

    m_lblConfigId.Create(L"Config ID (_dragonjeweltype):", WS_CHILD | WS_VISIBLE,
                         CRect(dluX(16), dluY(38), dluX(120), dluY(50)), this);
    m_lblConfigId.SetFont(&m_fonts.secondary);

    m_edtConfigId.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER,
                         CRect(dluX(124), dluY(36), dluX(170), dluY(50)), this, IDC_STATIC);
    m_edtConfigId.SetFont(&m_fonts.regular);
    m_edtConfigId.SetWindowTextW(std::to_wstring(m_currentConfigId).c_str());

    m_btnApplyConfig.Create(L"Apply", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            CRect(dluX(174), dluY(36), dluX(212), dluY(50)), this, IDC_DJ_APPLY_CFG);
    m_btnApplyConfig.SetFont(&m_fonts.regular);

    // 4 Sockets
    const UINT btnIds[4] = { IDC_DJ_SOCKET_1, IDC_DJ_SOCKET_2, IDC_DJ_SOCKET_3, IDC_DJ_SOCKET_4 };
    for (int i = 0; i < 4; ++i) {
        const int y = 56 + i * 40;
        m_lblSlotInfo[i].Create(std::format(L"Slot {}: [None]", i + 1).c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                                CRect(dluX(16), dluY(y), dluX(150), dluY(y + 28)), this);
        m_lblSlotInfo[i].SetFont(&m_fonts.regular);

        m_btnSocket[i].Create(L"Socket", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              CRect(dluX(154), dluY(y + 4), dluX(212), dluY(y + 24)), this, btnIds[i]);
        m_btnSocket[i].SetFont(&m_fonts.regular);
    }

    m_btnClearSockets.Create(L"Clear All Sockets", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             CRect(dluX(16), dluY(222), dluX(212), dluY(240)), this, IDC_DJ_CLEAR_ALL);
    m_btnClearSockets.SetFont(&m_fonts.regular);

    // Right pane: Jade Browser / Equippable Items
    const int rightX = 228;
    const int rightW = 282;

    if (!m_isJewelItem) {
        m_grpBrowser.Create(L"Compatible Dragon Jewels", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                            CRect(dluX(rightX), dluY(24), dluX(rightX + rightW), dluY(250)), this);

        m_lblTypeFilter.Create(L"Type:", WS_CHILD | WS_VISIBLE,
                               CRect(dluX(rightX + 8), dluY(38), dluX(rightX + 35), dluY(50)), this);
        m_lblTypeFilter.SetFont(&m_fonts.secondary);

        m_cmbTypeFilter.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                               CRect(dluX(rightX + 38), dluY(36), dluX(rightX + 130), dluY(180)), this, IDC_DJ_TYPE_COMBO);
        m_cmbTypeFilter.SetFont(&m_fonts.regular);
        m_cmbTypeFilter.AddString(L"All Jade Types");
        m_cmbTypeFilter.AddString(L"1 - Offensive / Attack");
        m_cmbTypeFilter.AddString(L"2 - Defensive / Armor");
        m_cmbTypeFilter.AddString(L"3 - Skill / Special");
        m_cmbTypeFilter.SetCurSel(0);

        m_lblRankFilter.Create(L"Rank:", WS_CHILD | WS_VISIBLE,
                               CRect(dluX(rightX + 138), dluY(38), dluX(rightX + 165), dluY(50)), this);
        m_lblRankFilter.SetFont(&m_fonts.secondary);

        m_cmbRankFilter.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                               CRect(dluX(rightX + 168), dluY(36), dluX(rightX + 274), dluY(180)), this, IDC_DJ_RANK_COMBO);
        m_cmbRankFilter.SetFont(&m_fonts.regular);
        m_cmbRankFilter.AddString(L"All Ranks");
        m_cmbRankFilter.AddString(L"Normal");
        m_cmbRankFilter.AddString(L"Magic");
        m_cmbRankFilter.AddString(L"Rare");
        m_cmbRankFilter.AddString(L"Epic");
        m_cmbRankFilter.AddString(L"Unique");
        m_cmbRankFilter.AddString(L"Legend");
        m_cmbRankFilter.SetCurSel(0);

        m_lstJewels.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                           CRect(dluX(rightX + 6), dluY(56), dluX(rightX + rightW - 6), dluY(242)), this, IDC_DJ_JEWEL_LIST);
        m_lstJewels.SetExtendedStyle(m_lstJewels.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        m_lstJewels.SetFont(&m_fonts.regular);
        m_lstJewels.InsertColumn(0, L"ID", LVCFMT_LEFT, dluX(45));
        m_lstJewels.InsertColumn(1, L"Name", LVCFMT_LEFT, dluX(85));
        m_lstJewels.InsertColumn(2, L"Rank", LVCFMT_LEFT, dluX(35));
        m_lstJewels.InsertColumn(3, L"Lv", LVCFMT_RIGHT, dluX(25));
        m_lstJewels.InsertColumn(4, L"Type", LVCFMT_LEFT, dluX(45));
        m_lstJewels.InsertColumn(5, L"Element", LVCFMT_LEFT, dluX(45));
    } else {
        m_grpEquip.Create(L"Compatible Equipment in Database", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                          CRect(dluX(rightX), dluY(24), dluX(rightX + rightW), dluY(250)), this);

        m_lstEquip.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                          CRect(dluX(rightX + 6), dluY(38), dluX(rightX + rightW - 6), dluY(242)), this, IDC_DJ_EQUIP_LIST);
        m_lstEquip.SetExtendedStyle(m_lstEquip.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        m_lstEquip.SetFont(&m_fonts.regular);
        m_lstEquip.InsertColumn(0, L"ID", LVCFMT_LEFT, dluX(45));
        m_lstEquip.InsertColumn(1, L"Item Name", LVCFMT_LEFT, dluX(110));
        m_lstEquip.InsertColumn(2, L"Rank", LVCFMT_LEFT, dluX(35));
        m_lstEquip.InsertColumn(3, L"Lv", LVCFMT_RIGHT, dluX(25));
        m_lstEquip.InsertColumn(4, L"Sockets", LVCFMT_LEFT, dluX(65));
    }

    // Bottom pane: Simulated Combined Combat Stats
    m_grpNetStats.Create(L"Simulated Net Combat Stats (Item + Socketed Jades)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                         CRect(dluX(10), dluY(256), dluX(440), dluY(348)), this);

    m_lblNetStats.Create(L"No socketed jades.", WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                         CRect(dluX(18), dluY(270), dluX(432), dluY(340)), this);
    m_lblNetStats.SetFont(&m_fonts.regular);

    m_btnClose.Create(L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(450), dluY(324), dluX(510), dluY(346)), this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    RefreshSocketView();
    if (!m_isJewelItem) {
        RefreshJewelList();
    } else {
        RefreshCompatibleEquipList();
    }
    RefreshNetStats();

    return TRUE;
}

void CDragonJewelStudioDlg::RefreshSocketView() {
    const auto* cfg = m_engine.Items().GetItemJewelSlots(m_currentConfigId);
    for (int i = 0; i < 4; ++i) {
        int sType = cfg ? cfg->slots[static_cast<size_t>(i)] : 0;
        std::wstring slotName = L"[Locked / None]";
        if (sType == 1) slotName = L"[Attack Jade]";
        else if (sType == 2) slotName = L"[Defense Jade]";
        else if (sType == 3) slotName = L"[Skill Jade]";

        std::wstring txt = std::format(L"Slot {}: {}", i + 1, slotName);
        const int32_t jid = m_socketedJewelIds[static_cast<size_t>(i)];
        if (jid > 0) {
            const auto* jItem = m_engine.Items().GetItem(jid);
            if (jItem) {
                txt += std::format(L"\n  -> #{} {}", jid, Utf8ToWide(jItem->name).GetString());
            }
        }
        m_lblSlotInfo[i].SetWindowTextW(txt.c_str());
        m_btnSocket[i].EnableWindow(sType > 0);
    }
}

void CDragonJewelStudioDlg::RefreshJewelList() {
    if (!m_lstJewels.GetSafeHwnd()) return;
    m_lstJewels.DeleteAllItems();

    const int selType = m_cmbTypeFilter.GetCurSel(); // 0=All, 1=Atk, 2=Def, 3=Skl
    const int selRank = m_cmbRankFilter.GetCurSel() - 1; // -1=All, 0=Norm..

    const auto jewels = m_engine.Items().GetCompatibleJewels(selType, 999, selRank);
    int row = 0;
    for (const auto& j : jewels) {
        const auto* item = m_engine.Items().GetItem(j.itemId);
        if (!item) continue;

        const int idx = m_lstJewels.InsertItem(row++, std::to_wstring(j.itemId).c_str());
        m_lstJewels.SetItemText(idx, 1, Utf8ToWide(item->name));
        m_lstJewels.SetItemText(idx, 2, Utf8ToWide(Data::ItemManager::RankName(item->rank)));
        m_lstJewels.SetItemText(idx, 3, std::to_wstring(item->levelLimit).c_str());
        m_lstJewels.SetItemText(idx, 4, Utf8ToWide(j.GetTypeName()));
        m_lstJewels.SetItemText(idx, 5, Utf8ToWide(j.GetElementName()));
        m_lstJewels.SetItemData(idx, static_cast<DWORD_PTR>(j.itemId));
    }
}

void CDragonJewelStudioDlg::RefreshCompatibleEquipList() {
    if (!m_lstEquip.GetSafeHwnd()) return;
    m_lstEquip.DeleteAllItems();

    const auto* info = m_engine.Items().GetDragonJewelInfo(m_selectedItemId);
    if (!info) return;

    const auto equipIds = m_engine.Items().GetCompatibleEquipmentForJewel(*info);
    int row = 0;
    for (int32_t eqId : equipIds) {
        const auto* eq = m_engine.Items().GetItem(eqId);
        if (!eq) continue;

        const int idx = m_lstEquip.InsertItem(row++, std::to_wstring(eqId).c_str());
        m_lstEquip.SetItemText(idx, 1, Utf8ToWide(eq->name));
        m_lstEquip.SetItemText(idx, 2, Utf8ToWide(Data::ItemManager::RankName(eq->rank)));
        m_lstEquip.SetItemText(idx, 3, std::to_wstring(eq->levelLimit).c_str());

        const auto* cfg = m_engine.Items().GetItemJewelSlots(eq->dragonJewelType);
        m_lstEquip.SetItemText(idx, 4, Utf8ToWide(cfg ? cfg->GetSummary() : "Sockets"));
        m_lstEquip.SetItemData(idx, static_cast<DWORD_PTR>(eqId));
    }
}

void CDragonJewelStudioDlg::RefreshNetStats() {
    std::unordered_map<int32_t, double> totalStats;
    auto addStats = [&](const Data::DNItem* it, const std::string& prefix) {
        if (!it) return;
        for (const auto& s : it->states) {
            if (s.type >= 0) {
                double val = 0.0;
                try { val = std::stod(s.minValue); } catch (...) {}
                totalStats[s.type] += val;
            }
        }
    };

    const auto* curItem = m_engine.Items().GetItem(m_selectedItemId);
    addStats(curItem, "Item");

    for (int32_t jid : m_socketedJewelIds) {
        if (jid > 0) {
            const auto* j = m_engine.Items().GetItem(jid);
            addStats(j, "Jade");
        }
    }

    if (totalStats.empty()) {
        m_lblNetStats.SetWindowTextW(L"No combat stats active on this item or socketed jades.");
        return;
    }

    std::wstring out = L"Net Combined Combat Stats:\n";
    int count = 0;
    for (const auto& [type, val] : totalStats) {
        const char* name = Data::ItemManager::StateDisplayName(type);
        if (count > 0 && count % 3 == 0) out += L"\n";
        else if (count > 0) out += L"  |  ";
        if (std::floor(val) == val) {
            out += std::format(L"{}: +{:L}", Utf8ToWide(name).GetString(), static_cast<int64_t>(val));
        } else {
            out += std::format(L"{}: +{:.1f}%", Utf8ToWide(name).GetString(), val);
        }
        ++count;
    }
    m_lblNetStats.SetWindowTextW(out.c_str());
}

void CDragonJewelStudioDlg::OnTypeFilterChanged() { RefreshJewelList(); }
void CDragonJewelStudioDlg::OnRankFilterChanged() { RefreshJewelList(); }

void CDragonJewelStudioDlg::OnJewelItemSelect(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
        m_selectedListJewelId = static_cast<int32_t>(m_lstJewels.GetItemData(pNMLV->iItem));
    }
    if (pResult) *pResult = 0;
}

void CDragonJewelStudioDlg::OnEquipItemSelect(NMHDR* pNMHDR, LRESULT* pResult) {
    POSITION pos = m_lstEquip.GetFirstSelectedItemPosition();
    if (pos) {
        const int idx = m_lstEquip.GetNextSelectedItem(pos);
        m_selectedItemId = static_cast<int32_t>(m_lstEquip.GetItemData(idx));
        m_isJewelItem = false;
        // Re-read config for this item
        const auto* it = m_engine.Items().GetItem(m_selectedItemId);
        if (it) m_currentConfigId = it->dragonJewelType;
        RefreshSocketView();
        RefreshJewelList();
        RefreshNetStats();
    }
    if (pResult) *pResult = 0;
}

void CDragonJewelStudioDlg::OnApplySocketConfig() {
    CStringW txt;
    m_edtConfigId.GetWindowTextW(txt);
    int newId = _wtoi(txt.GetString());
    if (newId >= 0 && m_selectedItemId > 0) {
        m_engine.Items().SetItemDragonJewelType(m_selectedItemId, newId);
        m_currentConfigId = newId;
        RefreshSocketView();
        RefreshNetStats();
    }
}

void CDragonJewelStudioDlg::OnSocketSlot1() {
    if (m_selectedListJewelId > 0) {
        m_socketedJewelIds[0] = m_selectedListJewelId;
        RefreshSocketView();
        RefreshNetStats();
    }
}

void CDragonJewelStudioDlg::OnSocketSlot2() {
    if (m_selectedListJewelId > 0) {
        m_socketedJewelIds[1] = m_selectedListJewelId;
        RefreshSocketView();
        RefreshNetStats();
    }
}

void CDragonJewelStudioDlg::OnSocketSlot3() {
    if (m_selectedListJewelId > 0) {
        m_socketedJewelIds[2] = m_selectedListJewelId;
        RefreshSocketView();
        RefreshNetStats();
    }
}

void CDragonJewelStudioDlg::OnSocketSlot4() {
    if (m_selectedListJewelId > 0) {
        m_socketedJewelIds[3] = m_selectedListJewelId;
        RefreshSocketView();
        RefreshNetStats();
    }
}

void CDragonJewelStudioDlg::OnClearAllSockets() {
    m_socketedJewelIds.fill(0);
    RefreshSocketView();
    RefreshNetStats();
}

void CDragonJewelStudioDlg::OnOK() {
    CDialog::OnOK();
}


// ============================================================================
// CSetItemStudioDlg
// ============================================================================

BEGIN_MESSAGE_MAP(CSetItemStudioDlg, CThemedDialog)
    ON_EN_CHANGE(IDC_SET_SEARCH, &CSetItemStudioDlg::OnSearchSetTextChanged)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_SET_LIST, &CSetItemStudioDlg::OnSetSelect)
    ON_NOTIFY(NM_CLICK, IDC_SET_PIECES_LIST, &CSetItemStudioDlg::OnPieceItemClick)
    ON_NOTIFY(NM_DBLCLK, IDC_SET_PIECES_LIST, &CSetItemStudioDlg::OnPieceItemDblClk)
    ON_BN_CLICKED(IDC_SET_TOGGLE_ALL, &CSetItemStudioDlg::OnToggleAllPieces)
    ON_BN_CLICKED(IDC_SET_JUMP_BTN, &CSetItemStudioDlg::OnJumpToItem)
END_MESSAGE_MAP()

CSetItemStudioDlg::CSetItemStudioDlg(Engine::WorkspaceEngine& engine, int32_t initialSetId, CWnd* parent)
    : m_tmpl(L"Set Item Bonus Inspector & Simulator", 580, 420, WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
      m_engine(engine),
      m_currentSetId(initialSetId) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CSetItemStudioDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);

    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int clientW = dluX(580);
    const int clientH = dluY(420);
    CRect curClient, curWin;
    GetClientRect(&curClient);
    GetWindowRect(&curWin);
    const int extraW = curWin.Width() - curClient.Width();
    const int extraH = curWin.Height() - curClient.Height();
    SetWindowPos(nullptr, 0, 0, clientW + extraW, clientH + extraH, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_lblHeader.Create(L"Dragon Nest Equipment Sets & Tiered Set Bonuses", WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                       CRect(dluX(10), dluY(8), dluX(530), dluY(20)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    // Left pane: Sets list
    m_grpSets.Create(L"All Sets", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                     CRect(dluX(10), dluY(24), dluX(180), dluY(320)), this);

    m_edtSearchSet.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                          CRect(dluX(16), dluY(38), dluX(174), dluY(52)), this, IDC_SET_SEARCH);
    m_edtSearchSet.SetFont(&m_fonts.regular);

    m_lstSets.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                     CRect(dluX(16), dluY(56), dluX(174), dluY(312)), this, IDC_SET_LIST);
    m_lstSets.SetExtendedStyle(m_lstSets.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_lstSets.SetFont(&m_fonts.regular);
    m_lstSets.InsertColumn(0, L"Set ID", LVCFMT_LEFT, dluX(40));
    m_lstSets.InsertColumn(1, L"Set Name", LVCFMT_LEFT, dluX(90));
    m_lstSets.InsertColumn(2, L"Pcs", LVCFMT_RIGHT, dluX(25));

    // Right pane top: Member Pieces
    const int rightX = 188;
    const int rightW = 342;

    m_grpPieces.Create(L"Set Member Pieces", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                       CRect(dluX(rightX), dluY(24), dluX(rightX + rightW), dluY(140)), this);

    m_lstPieces.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                       CRect(dluX(rightX + 6), dluY(38), dluX(rightX + rightW - 6), dluY(116)), this, IDC_SET_PIECES_LIST);
    m_lstPieces.SetExtendedStyle(m_lstPieces.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
    m_lstPieces.SetFont(&m_fonts.regular);
    m_lstPieces.InsertColumn(0, L"Equipped", LVCFMT_LEFT, dluX(45));
    m_lstPieces.InsertColumn(1, L"ID", LVCFMT_LEFT, dluX(45));
    m_lstPieces.InsertColumn(2, L"Piece Name", LVCFMT_LEFT, dluX(120));
    m_lstPieces.InsertColumn(3, L"Rank", LVCFMT_LEFT, dluX(40));
    m_lstPieces.InsertColumn(4, L"Lv", LVCFMT_RIGHT, dluX(25));
    m_lstPieces.InsertColumn(5, L"Slot", LVCFMT_LEFT, dluX(50));

    m_btnToggleAll.Create(L"Toggle Equip All", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(dluX(rightX + 6), dluY(120), dluX(rightX + 80), dluY(136)), this, IDC_SET_TOGGLE_ALL);
    m_btnToggleAll.SetFont(&m_fonts.secondary);

    m_btnJumpToItem.Create(L"Jump to Item in Studio", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           CRect(dluX(rightX + 84), dluY(120), dluX(rightX + 180), dluY(136)), this, IDC_SET_JUMP_BTN);
    m_btnJumpToItem.SetFont(&m_fonts.secondary);

    // Right pane middle: Tiered Bonuses
    m_grpBonuses.Create(L"Tiered Set Piece Bonuses", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        CRect(dluX(rightX), dluY(144), dluX(rightX + rightW), dluY(260)), this);

    m_lstBonuses.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                        CRect(dluX(rightX + 6), dluY(158), dluX(rightX + rightW - 6), dluY(238)), this, IDC_SET_BONUSES_LIST);
    m_lstBonuses.SetExtendedStyle(m_lstBonuses.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_lstBonuses.SetFont(&m_fonts.regular);
    m_lstBonuses.InsertColumn(0, L"Requirement", LVCFMT_LEFT, dluX(65));
    m_lstBonuses.InsertColumn(1, L"Stat / Attribute", LVCFMT_LEFT, dluX(140));
    m_lstBonuses.InsertColumn(2, L"Bonus Value", LVCFMT_RIGHT, dluX(110));

    m_lblSkillBonus.Create(L"Skill / Transform Effects: (None)", WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                           CRect(dluX(rightX + 8), dluY(242), dluX(rightX + rightW - 8), dluY(256)), this);
    m_lblSkillBonus.SetFont(&m_fonts.secondary);

    // Right pane bottom: Active Simulator Summary
    m_grpSimSummary.Create(L"Active Set Bonus Simulator Total", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                           CRect(dluX(rightX), dluY(264), dluX(rightX + rightW), dluY(320)), this);

    m_lblSimSummary.Create(L"Equip pieces above to simulate active tiered bonuses.", WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                           CRect(dluX(rightX + 8), dluY(278), dluX(rightX + rightW - 8), dluY(312)), this);
    m_lblSimSummary.SetFont(&m_fonts.regular);

    m_btnClose.Create(L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(470), dluY(328), dluX(530), dluY(350)), this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    PopulateSetList("");
    if (m_currentSetId > 0) {
        DisplaySet(m_currentSetId);
    } else if (m_lstSets.GetItemCount() > 0) {
        m_lstSets.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        DisplaySet(static_cast<int32_t>(m_lstSets.GetItemData(0)));
    }

    return TRUE;
}

void CSetItemStudioDlg::PopulateSetList(std::string_view filter) {
    if (!m_lstSets.GetSafeHwnd()) return;
    m_lstSets.DeleteAllItems();

    const auto& setIds = m_engine.Items().GetAllSetItemIds();
    int row = 0;
    for (int32_t sid : setIds) {
        const auto* grp = m_engine.Items().GetSetItemGroup(sid);
        if (!grp) continue;

        if (!filter.empty()) {
            std::string sidStr = std::to_string(sid);
            if (sidStr.find(filter) == std::string::npos &&
                grp->setName.find(filter) == std::string::npos) {
                continue;
            }
        }

        const int idx = m_lstSets.InsertItem(row++, std::to_wstring(sid).c_str());
        std::wstring nameW = grp->setName.empty() ? L"(Unnamed Set)" : Utf8ToWide(grp->setName).GetString();
        m_lstSets.SetItemText(idx, 1, nameW.c_str());
        m_lstSets.SetItemText(idx, 2, std::to_wstring(grp->memberItemIds.size()).c_str());
        m_lstSets.SetItemData(idx, static_cast<DWORD_PTR>(sid));
    }
}

void CSetItemStudioDlg::DisplaySet(int32_t setId) {
    m_currentSetId = setId;
    const auto* grp = m_engine.Items().GetSetItemGroup(setId);
    if (!grp) return;

    // Display Pieces
    m_lstPieces.DeleteAllItems();
    m_currentMemberIds = grp->memberItemIds;
    m_pieceEquipped.assign(m_currentMemberIds.size(), true); // Default all equipped

    int row = 0;
    for (size_t i = 0; i < m_currentMemberIds.size(); ++i) {
        const int32_t itemId = m_currentMemberIds[i];
        const auto* item = m_engine.Items().GetItem(itemId);
        if (!item) continue;

        const int idx = m_lstPieces.InsertItem(row++, L"");
        m_lstPieces.SetCheck(idx, TRUE);
        m_lstPieces.SetItemText(idx, 1, std::to_wstring(itemId).c_str());
        m_lstPieces.SetItemText(idx, 2, Utf8ToWide(item->name));
        m_lstPieces.SetItemText(idx, 3, Utf8ToWide(Data::ItemManager::RankName(item->rank)));
        m_lstPieces.SetItemText(idx, 4, std::to_wstring(item->levelLimit).c_str());
        m_lstPieces.SetItemText(idx, 5, Utf8ToWide(std::string(Data::ItemManager::EquipSubTypeName(*item))));
        m_lstPieces.SetItemData(idx, static_cast<DWORD_PTR>(itemId));
    }

    // Display Bonuses
    m_lstBonuses.DeleteAllItems();
    row = 0;
    for (const auto& b : grp->bonuses) {
        const int idx = m_lstBonuses.InsertItem(row++, std::format(L"{} Pieces", b.needNum).c_str());
        m_lstBonuses.SetItemText(idx, 1, Utf8ToWide(Data::ItemManager::StateDisplayName(b.stateType)));
        m_lstBonuses.SetItemText(idx, 2, std::format(L"+{}", Utf8ToWide(b.stateValue).GetString()).c_str());
    }

    // Display Skill bonuses
    std::wstring extra;
    for (const auto& sk : grp->skillBonuses) {
        if (!extra.empty()) extra += L" | ";
        extra += std::format(L"{} Pcs: Skill #{} Lv+{}", sk.needNum, sk.skillId, sk.skillLevel);
    }
    if (grp->transformItemId > 0) {
        if (!extra.empty()) extra += L" | ";
        extra += std::format(L"Transformation: Item #{}", grp->transformItemId);
    }
    if (extra.empty()) extra = L"Skill / Transform Effects: (None)";
    m_lblSkillBonus.SetWindowTextW(extra.c_str());

    UpdateSimulatorTotal();
}

void CSetItemStudioDlg::UpdateSimulatorTotal() {
    int equippedCount = 0;
    for (int i = 0; i < m_lstPieces.GetItemCount(); ++i) {
        if (m_lstPieces.GetCheck(i)) ++equippedCount;
    }

    const auto* grp = m_engine.Items().GetSetItemGroup(m_currentSetId);
    if (!grp) return;

    std::unordered_map<int32_t, double> totals;
    for (const auto& b : grp->bonuses) {
        if (b.needNum <= equippedCount) {
            double v = 0.0;
            try { v = std::stod(b.stateValue); } catch (...) {}
            totals[b.stateType] += v;
        }
    }

    std::wstring out = std::format(L"Active: {}/{} Pieces Equipped\n", equippedCount, grp->memberItemIds.size());
    if (totals.empty()) {
        out += L"Equip at least 2 pieces to activate tiered set bonuses.";
    } else {
        int c = 0;
        for (const auto& [type, val] : totals) {
            if (c > 0) out += L"  |  ";
            const char* name = Data::ItemManager::StateDisplayName(type);
            if (std::floor(val) == val) {
                out += std::format(L"{}: +{:L}", Utf8ToWide(name).GetString(), static_cast<int64_t>(val));
            } else {
                out += std::format(L"{}: +{:.1f}%", Utf8ToWide(name).GetString(), val);
            }
            ++c;
        }
    }
    m_lblSimSummary.SetWindowTextW(out.c_str());
}

void CSetItemStudioDlg::OnSearchSetTextChanged() {
    CStringW txt;
    m_edtSearchSet.GetWindowTextW(txt);
    std::string s = WideToUtf8(txt.GetString());
    PopulateSetList(s);
}

void CSetItemStudioDlg::OnSetSelect(NMHDR* pNMHDR, LRESULT* pResult) {
    auto* pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
        int32_t sid = static_cast<int32_t>(m_lstSets.GetItemData(pNMLV->iItem));
        DisplaySet(sid);
    }
    if (pResult) *pResult = 0;
}

void CSetItemStudioDlg::OnPieceItemClick(NMHDR* pNMHDR, LRESULT* pResult) {
    // Delay slightly to let check state update
    PostMessageW(WM_COMMAND, MAKEWPARAM(0, 9999), 0);
    UpdateSimulatorTotal();
    if (pResult) *pResult = 0;
}

void CSetItemStudioDlg::OnPieceItemDblClk(NMHDR* pNMHDR, LRESULT* pResult) {
    OnJumpToItem();
    if (pResult) *pResult = 0;
}

void CSetItemStudioDlg::OnToggleAllPieces() {
    bool anyUnchecked = false;
    for (int i = 0; i < m_lstPieces.GetItemCount(); ++i) {
        if (!m_lstPieces.GetCheck(i)) { anyUnchecked = true; break; }
    }
    for (int i = 0; i < m_lstPieces.GetItemCount(); ++i) {
        m_lstPieces.SetCheck(i, anyUnchecked ? TRUE : FALSE);
    }
    UpdateSimulatorTotal();
}

void CSetItemStudioDlg::OnJumpToItem() {
    POSITION pos = m_lstPieces.GetFirstSelectedItemPosition();
    if (pos) {
        const int idx = m_lstPieces.GetNextSelectedItem(pos);
        m_jumpToItemId = static_cast<int32_t>(m_lstPieces.GetItemData(idx));
        EndDialog(IDOK);
    }
}

void CSetItemStudioDlg::OnOK() {
    CDialog::OnOK();
}


// ============================================================================
// CEnchantStudioDlg
// ============================================================================

BEGIN_MESSAGE_MAP(CEnchantStudioDlg, CThemedDialog)
    ON_BN_CLICKED(IDC_ENC_RUN_BTN, &CEnchantStudioDlg::OnRunSimulation)
END_MESSAGE_MAP()

CEnchantStudioDlg::CEnchantStudioDlg(Engine::WorkspaceEngine& engine, int32_t enchantId, int32_t currentItemId, CWnd* parent)
    : m_tmpl(L"Enchantment Curve & Odds Simulator", 580, 420, WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
      m_engine(engine),
      m_enchantId(enchantId),
      m_itemId(currentItemId) {
    InitModalIndirect(m_tmpl.Get(), parent);
    m_curve = m_engine.Items().GetEnchantCurve(m_enchantId);
}

BOOL CEnchantStudioDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);

    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int clientW = dluX(580);
    const int clientH = dluY(420);
    CRect curClient, curWin;
    GetClientRect(&curClient);
    GetWindowRect(&curWin);
    const int extraW = curWin.Width() - curClient.Width();
    const int extraH = curWin.Height() - curClient.Height();
    SetWindowPos(nullptr, 0, 0, clientW + extraW, clientH + extraH, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    const auto* it = m_engine.Items().GetItem(m_itemId);
    std::wstring hdr = std::format(L"Enchant Progression Curve #{} (Total Steps: {})",
                                   m_enchantId, m_curve.steps.size());
    if (it) {
        hdr = std::format(L"#{} '{}' - Enchant Curve #{} (Max +{})",
                          it->id, Utf8ToWide(it->name).GetString(), m_enchantId,
                          m_curve.steps.empty() ? 0 : m_curve.steps.back().level);
    }
    m_lblHeader.Create(hdr.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                       CRect(dluX(10), dluY(8), dluX(530), dluY(20)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    // Center: Full Progression Table
    m_grpTable.Create(L"Upgrade Progression Matrix (+1 to +20)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      CRect(dluX(10), dluY(24), dluX(530), dluY(200)), this);

    m_lstSteps.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                      CRect(dluX(16), dluY(38), dluX(524), dluY(194)), this, IDC_ENC_STEPS_LIST);
    m_lstSteps.SetExtendedStyle(m_lstSteps.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_lstSteps.SetFont(&m_fonts.regular);
    m_lstSteps.InsertColumn(0, L"Level", LVCFMT_LEFT, dluX(30));
    m_lstSteps.InsertColumn(1, L"Success %", LVCFMT_RIGHT, dluX(45));
    m_lstSteps.InsertColumn(2, L"Break %", LVCFMT_RIGHT, dluX(40));
    m_lstSteps.InsertColumn(3, L"Penalty", LVCFMT_LEFT, dluX(45));
    m_lstSteps.InsertColumn(4, L"Gold Cost", LVCFMT_RIGHT, dluX(55));
    m_lstSteps.InsertColumn(5, L"Required Materials", LVCFMT_LEFT, dluX(145));
    m_lstSteps.InsertColumn(6, L"Stat Boosts", LVCFMT_LEFT, dluX(145));

    // Bottom pane: Monte Carlo Simulator
    m_grpSim.Create(L"Monte Carlo Upgrade Cost & Odds Simulator", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                    CRect(dluX(10), dluY(206), dluX(530), dluY(326)), this);

    m_lblTargetLvl.Create(L"Target Level:", WS_CHILD | WS_VISIBLE,
                          CRect(dluX(16), dluY(220), dluX(65), dluY(232)), this);
    m_lblTargetLvl.SetFont(&m_fonts.secondary);

    m_cmbTargetLvl.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                          CRect(dluX(68), dluY(218), dluX(115), dluY(150)), this, IDC_ENC_TARGET_LVL);
    m_cmbTargetLvl.SetFont(&m_fonts.regular);

    m_chkJelly.Create(L"Use Item Protection Jelly (Prevents Destruction)",
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                      CRect(dluX(125), dluY(218), dluX(280), dluY(232)), this, IDC_ENC_JELLY_CHK);
    m_chkJelly.SetFont(&m_fonts.regular);
    m_chkJelly.SetCheck(BST_CHECKED);

    m_lblTrials.Create(L"Trials:", WS_CHILD | WS_VISIBLE,
                       CRect(dluX(290), dluY(220), dluX(315), dluY(232)), this);
    m_lblTrials.SetFont(&m_fonts.secondary);

    m_cmbTrials.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                       CRect(dluX(318), dluY(218), dluX(430), dluY(120)), this, IDC_ENC_TRIALS_COMBO);
    m_cmbTrials.SetFont(&m_fonts.regular);
    m_cmbTrials.AddString(L"1 Run (Step Log)");
    m_cmbTrials.AddString(L"100 Runs");
    m_cmbTrials.AddString(L"1,000 Runs");
    m_cmbTrials.AddString(L"10,000 Runs");
    m_cmbTrials.SetCurSel(1);

    m_btnRunSim.Create(L"Simulate", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(dluX(440), dluY(216), dluX(522), dluY(234)), this, IDC_ENC_RUN_BTN);
    m_btnRunSim.SetFont(&m_fonts.bold);

    m_edtTelemetry.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                          CRect(dluX(16), dluY(238), dluX(524), dluY(320)), this, IDC_ENC_TELEMETRY);
    m_edtTelemetry.SetFont(&m_fonts.mono);
    m_edtTelemetry.SetWindowTextW(L"Select a target enhancement level and click Simulate to compute expected costs.\r\n");

    m_btnClose.Create(L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(460), dluY(332), dluX(530), dluY(352)), this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    PopulateCurveTable();

    return TRUE;
}

void CEnchantStudioDlg::PopulateCurveTable() {
    m_lstSteps.DeleteAllItems();
    m_cmbTargetLvl.ResetContent();

    int row = 0;
    for (const auto& st : m_curve.steps) {
        const int idx = m_lstSteps.InsertItem(row++, std::format(L"+{}", st.level).c_str());
        m_lstSteps.SetItemText(idx, 1, std::format(L"{:.1f}%", st.successRatio * 100.0f).c_str());
        m_lstSteps.SetItemText(idx, 2, std::format(L"{:.1f}%", st.breakRatio * 100.0f).c_str());

        std::wstring pen = L"None";
        if (st.minDown > 0 || st.maxDown > 0) {
            pen = std::format(L"-{} to -{}", st.minDown, st.maxDown);
        }
        m_lstSteps.SetItemText(idx, 3, pen.c_str());

        // Gold formatted
        const int64_t g = st.needCoin / 10000;
        const int64_t s = (st.needCoin % 10000) / 100;
        const int64_t c = st.needCoin % 100;
        m_lstSteps.SetItemText(idx, 4, std::format(L"{}g {}s {}c", g, s, c).c_str());

        // Materials
        std::wstring mats;
        for (size_t m = 0; m < 5; ++m) {
            const int32_t matId = st.needItemIds[m];
            const int32_t matCount = st.needItemCounts[m];
            if (matId > 0 && matCount > 0) {
                if (!mats.empty()) mats += L", ";
                const auto* mItem = m_engine.Items().GetItem(matId);
                mats += std::format(L"{}x {}", matCount, mItem ? Utf8ToWide(mItem->name).GetString() : std::to_wstring(matId));
            }
        }
        m_lstSteps.SetItemText(idx, 5, mats.empty() ? L"(Free)" : mats.c_str());

        // Stats
        std::wstring stStr;
        for (const auto& sb : st.stats) {
            if (!stStr.empty()) stStr += L", ";
            stStr += std::format(L"+{} {}", Utf8ToWide(sb.stateValue).GetString(),
                                 Utf8ToWide(Data::ItemManager::StateDisplayName(sb.stateType)).GetString());
        }
        m_lstSteps.SetItemText(idx, 6, stStr.c_str());

        m_cmbTargetLvl.AddString(std::format(L"+{}", st.level).c_str());
    }

    if (m_cmbTargetLvl.GetCount() > 0) {
        m_cmbTargetLvl.SetCurSel(std::min(9, m_cmbTargetLvl.GetCount() - 1)); // Default +10
    }
}

void CEnchantStudioDlg::OnRunSimulation() {
    const int selIdx = m_cmbTargetLvl.GetCurSel();
    if (selIdx < 0 || selIdx >= static_cast<int>(m_curve.steps.size())) return;

    const int targetLvl = m_curve.steps[static_cast<size_t>(selIdx)].level;
    const bool useJelly = m_chkJelly.GetCheck() == BST_CHECKED;

    int trials = 100;
    const int tSel = m_cmbTrials.GetCurSel();
    if (tSel == 0) trials = 1;
    else if (tSel == 1) trials = 100;
    else if (tSel == 2) trials = 1000;
    else if (tSel == 3) trials = 10000;

    RunMonteCarlo(trials, targetLvl, useJelly);
}

void CEnchantStudioDlg::RunMonteCarlo(int trials, int targetLvl, bool useJelly) {
    if (m_curve.steps.empty()) return;

    std::mt19937_64 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int64_t totalAttempts = 0;
    int64_t totalGold = 0;
    int64_t totalDestroyed = 0;
    int64_t totalJellies = 0;
    std::unordered_map<int32_t, int64_t> totalMats;
    std::vector<int> attemptsPerTrial;
    attemptsPerTrial.reserve(static_cast<size_t>(trials));

    std::wstring logOutput;

    for (int t = 0; t < trials; ++t) {
        int curLvl = 0;
        int attempts = 0;

        if (trials == 1) {
            logOutput += std::format(L"--- Upgrade Attempt to +{} ---\r\n", targetLvl);
        }

        while (curLvl < targetLvl) {
            ++attempts;
            // Find step for curLvl + 1
            const Data::EnchantStep* step = nullptr;
            for (const auto& s : m_curve.steps) {
                if (s.level == curLvl + 1) { step = &s; break; }
            }
            if (!step) break;

            totalGold += step->needCoin;
            for (size_t m = 0; m < 5; ++m) {
                if (step->needItemIds[m] > 0 && step->needItemCounts[m] > 0) {
                    totalMats[step->needItemIds[m]] += step->needItemCounts[m];
                }
            }
            if (useJelly && step->protectItemCount > 0) {
                totalJellies += step->protectItemCount;
            }

            const float roll = dist(rng);
            if (roll <= step->successRatio) {
                // Success!
                ++curLvl;
                if (trials == 1) {
                    logOutput += std::format(L"[Att #{:3d}] +{:2d} -> +{:2d}: SUCCESS! (Roll: {:.3f} <= {:.3f})\r\n",
                                             attempts, curLvl - 1, curLvl, roll, step->successRatio);
                }
            } else if (roll <= (step->successRatio + step->breakRatio)) {
                // Destruction attempt
                if (useJelly) {
                    if (trials == 1) {
                        logOutput += std::format(L"[Att #{:3d}] +{:2d} -> +{:2d}: PROTECTED FROM BREAK! (Jelly consumed)\r\n",
                                                 attempts, curLvl, curLvl + 1);
                    }
                    if (step->minDown > 0) {
                        curLvl = std::max(0, curLvl - step->minDown);
                    }
                } else {
                    ++totalDestroyed;
                    if (trials == 1) {
                        logOutput += std::format(L"[Att #{:3d}] +{:2d} -> +{:2d}: DESTROYED! Item shattered to fragments.\r\n",
                                                 attempts, curLvl, curLvl + 1);
                    }
                    curLvl = 0; // Reset
                }
            } else {
                // Failure downgrade
                int down = step->minDown > 0 ? step->minDown : 0;
                curLvl = std::max(0, curLvl - down);
                if (trials == 1) {
                    logOutput += std::format(L"[Att #{:3d}] +{:2d} -> +{:2d}: FAILED (Dropped to +{:2d})\r\n",
                                             attempts, curLvl + down, curLvl + down + 1, curLvl);
                }
            }

            if (attempts > 50000) break; // Safety cap
        }

        totalAttempts += attempts;
        attemptsPerTrial.push_back(attempts);
    }

    std::sort(attemptsPerTrial.begin(), attemptsPerTrial.end());
    const double avgAtt = static_cast<double>(totalAttempts) / trials;
    const double avgGold = static_cast<double>(totalGold) / trials;
    const int p50 = attemptsPerTrial[attemptsPerTrial.size() / 2];
    const int p90 = attemptsPerTrial[static_cast<size_t>(attemptsPerTrial.size() * 0.90)];
    const int p99 = attemptsPerTrial[static_cast<size_t>(attemptsPerTrial.size() * 0.99)];

    const int64_t g = static_cast<int64_t>(avgGold) / 10000;
    const int64_t s = (static_cast<int64_t>(avgGold) % 10000) / 100;
    const int64_t c = static_cast<int64_t>(avgGold) % 100;

    std::wstring rep;
    rep += std::format(L"Simulation Results ({} Trial{} to Target +{}):\r\n", trials, trials > 1 ? L"s" : L"", targetLvl);
    rep += std::format(L"  - Average Attempts:      {:.1f} (Median: {}, 90th percentile: {}, Worst: {})\r\n",
                       avgAtt, p50, p90, attemptsPerTrial.back());
    rep += std::format(L"  - Expected Gold Cost:    {}g {}s {}c\r\n", g, s, c);
    if (useJelly) {
        rep += std::format(L"  - Protection Jellies:    {:.1f} consumed (Items shattered: 0)\r\n",
                           static_cast<double>(totalJellies) / trials);
    } else {
        rep += std::format(L"  - Items Shattered:       {:.2f} average destructions\r\n",
                           static_cast<double>(totalDestroyed) / trials);
    }

    rep += L"  - Materials Consumed:\r\n";
    for (const auto& [matId, totalCnt] : totalMats) {
        const auto* mItem = m_engine.Items().GetItem(matId);
        std::wstring matDisplayName = mItem ? Utf8ToWide(mItem->name).GetString() : std::to_wstring(matId);
        rep += std::format(L"      * {}: {:.1f} avg\r\n", matDisplayName, static_cast<double>(totalCnt) / trials);
    }

    if (trials == 1) {
        rep += L"\r\nDetailed Action Log:\r\n";
        rep += logOutput;
    }

    m_edtTelemetry.SetWindowTextW(rep.c_str());
}

void CEnchantStudioDlg::OnOK() {
    CDialog::OnOK();
}

// ---------------------------------------------------------------------------
// Helpers for BMP and Clipboard Image I/O
// ---------------------------------------------------------------------------

namespace {

bool SaveBmp32(const std::filesystem::path& path, const Graphics::ImageBuffer& img) {
    if (img.width == 0 || img.height == 0 || img.rgbaData.empty()) return false;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + img.width * img.height * 4;

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = static_cast<LONG>(img.width);
    bih.biHeight = static_cast<LONG>(img.height); // Bottom-up
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = img.width * img.height * 4;

    f.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    f.write(reinterpret_cast<const char*>(&bih), sizeof(bih));

    std::vector<uint8_t> row(img.width * 4);
    for (int y = static_cast<int>(img.height) - 1; y >= 0; --y) {
        const uint8_t* srcRow = &img.rgbaData[static_cast<size_t>(y) * img.width * 4];
        for (uint32_t x = 0; x < img.width; ++x) {
            row[x * 4 + 0] = srcRow[x * 4 + 2]; // B
            row[x * 4 + 1] = srcRow[x * 4 + 1]; // G
            row[x * 4 + 2] = srcRow[x * 4 + 0]; // R
            row[x * 4 + 3] = srcRow[x * 4 + 3]; // A
        }
        f.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
    }
    return f.good();
}

bool LoadBmp32(const std::filesystem::path& path, Graphics::ImageBuffer& outImg) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    BITMAPFILEHEADER bfh{};
    if (!f.read(reinterpret_cast<char*>(&bfh), sizeof(bfh))) return false;
    if (bfh.bfType != 0x4D42) return false;

    BITMAPINFOHEADER bih{};
    if (!f.read(reinterpret_cast<char*>(&bih), sizeof(bih))) return false;
    if (bih.biWidth <= 0 || bih.biHeight == 0) return false;

    const bool bottomUp = (bih.biHeight > 0);
    const uint32_t w = static_cast<uint32_t>(bih.biWidth);
    const uint32_t h = static_cast<uint32_t>(std::abs(bih.biHeight));
    const uint16_t bpp = bih.biBitCount;

    if (bpp != 24 && bpp != 32) return false;

    f.seekg(bfh.bfOffBits, std::ios::beg);
    const size_t rowBytes = ((static_cast<size_t>(w) * bpp + 31) / 32) * 4;
    std::vector<uint8_t> rowBuf(rowBytes);

    outImg.width = w;
    outImg.height = h;
    outImg.rgbaData.resize(static_cast<size_t>(w) * h * 4);

    for (uint32_t r = 0; r < h; ++r) {
        const uint32_t y = bottomUp ? (h - 1 - r) : r;
        if (!f.read(reinterpret_cast<char*>(rowBuf.data()), static_cast<std::streamsize>(rowBytes))) return false;
        uint8_t* dst = &outImg.rgbaData[static_cast<size_t>(y) * w * 4];
        for (uint32_t x = 0; x < w; ++x) {
            if (bpp == 32) {
                dst[x * 4 + 0] = rowBuf[x * 4 + 2]; // R
                dst[x * 4 + 1] = rowBuf[x * 4 + 1]; // G
                dst[x * 4 + 2] = rowBuf[x * 4 + 0]; // B
                dst[x * 4 + 3] = rowBuf[x * 4 + 3]; // A
            } else {
                dst[x * 4 + 0] = rowBuf[x * 3 + 2]; // R
                dst[x * 4 + 1] = rowBuf[x * 3 + 1]; // G
                dst[x * 4 + 2] = rowBuf[x * 3 + 0]; // B
                dst[x * 4 + 3] = 255;
            }
        }
    }
    return true;
}

bool ReadClipboardBitmap(Graphics::ImageBuffer& outImg) {
    if (!::OpenClipboard(nullptr)) return false;
    HBITMAP hBmp = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
    if (!hBmp) {
        ::CloseClipboard();
        return false;
    }

    BITMAP bmp{};
    ::GetObjectW(hBmp, sizeof(BITMAP), &bmp);
    if (bmp.bmWidth <= 0 || bmp.bmHeight <= 0) {
        ::CloseClipboard();
        return false;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bmp.bmWidth;
    bi.bmiHeader.biHeight = -bmp.bmHeight; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = ::GetDC(nullptr);
    outImg.width = static_cast<uint32_t>(bmp.bmWidth);
    outImg.height = static_cast<uint32_t>(bmp.bmHeight);
    outImg.rgbaData.resize(static_cast<size_t>(outImg.width) * outImg.height * 4);

    std::vector<uint32_t> bgraPixels(static_cast<size_t>(outImg.width) * outImg.height);
    ::GetDIBits(hdc, hBmp, 0, static_cast<UINT>(bmp.bmHeight), bgraPixels.data(), &bi, DIB_RGB_COLORS);
    ::ReleaseDC(nullptr, hdc);
    ::CloseClipboard();

    for (size_t i = 0; i < bgraPixels.size(); ++i) {
        const uint32_t p = bgraPixels[i];
        outImg.rgbaData[i * 4 + 0] = static_cast<uint8_t>((p >> 16) & 0xFF); // R
        outImg.rgbaData[i * 4 + 1] = static_cast<uint8_t>((p >> 8) & 0xFF);  // G
        outImg.rgbaData[i * 4 + 2] = static_cast<uint8_t>(p & 0xFF);         // B
        outImg.rgbaData[i * 4 + 3] = 255;                                   // A
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// CActConverterDlg
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CActConverterDlg, CThemedDialog)
    ON_BN_CLICKED(IDC_ACT_BROWSE_IN, &CActConverterDlg::OnBrowseInput)
    ON_BN_CLICKED(IDC_ACT_CONVERT_SINGLE, &CActConverterDlg::OnConvertSingle)
    ON_BN_CLICKED(IDC_ACT_BROWSE_BATCH, &CActConverterDlg::OnBrowseBatchDir)
    ON_BN_CLICKED(IDC_ACT_CONVERT_BATCH, &CActConverterDlg::OnConvertBatch)
END_MESSAGE_MAP()

CActConverterDlg::CActConverterDlg(Engine::WorkspaceEngine& engine, CWnd* parent)
    : m_tmpl(L"Eternity ACT Version 6 -> Version 5 Converter", 580, 440, WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
      m_engine(engine) {
    InitModalIndirect(m_tmpl.Get(), parent);
}

BOOL CActConverterDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);

    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int clientW = dluX(580);
    const int clientH = dluY(440);
    CRect curClient, curWin;
    GetClientRect(&curClient);
    GetWindowRect(&curWin);
    const int extraW = curWin.Width() - curClient.Width();
    const int extraH = curWin.Height() - curClient.Height();
    SetWindowPos(nullptr, 0, 0, clientW + extraW, clientH + extraH, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_lblHeader.Create(L"Dragon Nest Eternity ACT Version 6 to Version 5 Conversion Tool",
                       WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                       CRect(dluX(10), dluY(8), dluX(570), dluY(22)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    // Card 1: Single File Conversion
    m_grpSingle.Create(L"Single .ACT File Conversion", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                       CRect(dluX(10), dluY(26), dluX(570), dluY(116)), this);

    m_lblInputFile.Create(L"Input ACT File:", WS_CHILD | WS_VISIBLE,
                          CRect(dluX(18), dluY(42), dluX(100), dluY(54)), this);
    m_lblInputFile.SetFont(&m_fonts.secondary);

    m_edtInputFile.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                          CRect(dluX(104), dluY(40), dluX(476), dluY(54)), this, IDC_ACT_INPUT_EDIT);
    m_edtInputFile.SetFont(&m_fonts.regular);

    m_btnBrowseInput.Create(L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            CRect(dluX(482), dluY(39), dluX(560), dluY(55)), this, IDC_ACT_BROWSE_IN);
    m_btnBrowseInput.SetFont(&m_fonts.regular);

    m_lblFileInfo.Create(L"Select an Eternity .act file to detect version and actions.",
                         WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                         CRect(dluX(18), dluY(60), dluX(476), dluY(84)), this);
    m_lblFileInfo.SetFont(&m_fonts.regular);

    m_btnConvertSingle.Create(L"Convert to v5", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              CRect(dluX(482), dluY(60), dluX(560), dluY(84)), this, IDC_ACT_CONVERT_SINGLE);
    m_btnConvertSingle.SetFont(&m_fonts.bold);

    // Card 2: Batch Directory Conversion
    m_grpBatch.Create(L"Batch Directory Conversion", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      CRect(dluX(10), dluY(122), dluX(570), dluY(210)), this);

    m_lblBatchDir.Create(L"Target Folder:", WS_CHILD | WS_VISIBLE,
                         CRect(dluX(18), dluY(138), dluX(100), dluY(150)), this);
    m_lblBatchDir.SetFont(&m_fonts.secondary);

    m_edtBatchDir.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                         CRect(dluX(104), dluY(136), dluX(476), dluY(150)), this, IDC_ACT_BATCH_EDIT);
    m_edtBatchDir.SetFont(&m_fonts.regular);
    if (!m_engine.Items().GetResourcePath().empty()) {
        m_edtBatchDir.SetWindowTextW((m_engine.Items().GetResourcePath() / "char").wstring().c_str());
    }

    m_btnBrowseBatch.Create(L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            CRect(dluX(482), dluY(135), dluX(560), dluY(151)), this, IDC_ACT_BROWSE_BATCH);
    m_btnBrowseBatch.SetFont(&m_fonts.regular);

    m_chkBackup.Create(L"Create .bak backup copy before overwriting original version 6 files",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                       CRect(dluX(18), dluY(156), dluX(476), dluY(170)), this, IDC_ACT_BACKUP_CHK);
    m_chkBackup.SetFont(&m_fonts.regular);
    m_chkBackup.SetCheck(BST_CHECKED);

    m_btnConvertBatch.Create(L"Scan & Convert All v6 Files", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             CRect(dluX(18), dluY(176), dluX(200), dluY(200)), this, IDC_ACT_CONVERT_BATCH);
    m_btnConvertBatch.SetFont(&m_fonts.bold);

    // Card 3: Status & Results Log
    m_grpResults.Create(L"Conversion Results & Diagnostic Log", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        CRect(dluX(10), dluY(216), dluX(570), dluY(400)), this);

    m_edtLog.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_BORDER |
                    ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                    CRect(dluX(18), dluY(232), dluX(560), dluY(390)), this, IDC_ACT_LOG);
    m_edtLog.SetFont(&m_fonts.mono);

    m_btnClose.Create(L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(500), dluY(410), dluX(570), dluY(432)), this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    AppendLog(L"ACT Converter initialized. Ready to process Dragon Nest .act files.");
    return TRUE;
}

void CActConverterDlg::AppendLog(const std::wstring& line) {
    if (!m_edtLog.GetSafeHwnd()) return;
    CString existing;
    m_edtLog.GetWindowTextW(existing);
    existing += line.c_str();
    existing += L"\r\n";
    m_edtLog.SetWindowTextW(existing);
    m_edtLog.LineScroll(m_edtLog.GetLineCount());
}

void CActConverterDlg::OnBrowseInput() {
    CFileDialog dlg(TRUE, L"act", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"Eternity Action Files (*.act)|*.act|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        const std::filesystem::path p = dlg.GetPathName().GetString();
        m_edtInputFile.SetWindowTextW(p.wstring().c_str());

        int32_t ver = 0, count = 0;
        bool isManifest = false;
        if (Graphics::ActConverter::ProbeActFile(p, ver, count, isManifest)) {
            if (isManifest) {
                m_lblFileInfo.SetWindowTextW(L"Status: Text manifest detected (references other .act files).");
            } else if (ver == 6) {
                m_lblFileInfo.SetWindowTextW(std::format(L"Status: Version 6 detected ({} actions). Ready to convert to v5.", count).c_str());
            } else if (ver == 5) {
                m_lblFileInfo.SetWindowTextW(std::format(L"Status: File is already Version 5 ({} actions).", count).c_str());
            } else {
                m_lblFileInfo.SetWindowTextW(std::format(L"Status: Version {} detected ({} actions).", ver, count).c_str());
            }
        } else {
            m_lblFileInfo.SetWindowTextW(L"Status: Invalid or unreadable .act file.");
        }
    }
}

void CActConverterDlg::OnConvertSingle() {
    CString pathStr;
    m_edtInputFile.GetWindowTextW(pathStr);
    const std::filesystem::path inPath = pathStr.GetString();
    if (inPath.empty() || !std::filesystem::exists(inPath)) {
        AfxMessageBox(L"Please select a valid .act file first.", MB_ICONWARNING);
        return;
    }

    const bool backup = (m_chkBackup.GetCheck() == BST_CHECKED);
    const auto res = Graphics::ActConverter::ConvertFileV6ToV5(inPath, inPath, backup);
    AppendLog(std::format(L"[{}] {}", res.success ? L"SUCCESS" : L"INFO", Utf8ToWide(res.message).GetString()));

    if (res.success) {
        m_lblFileInfo.SetWindowTextW(L"Status: Successfully converted to Version 5!");
        AfxMessageBox(std::format(L"File successfully converted to Version 5!\nActions: {}\nSaved: {} bytes",
                                  res.actionCount, (res.bytesBefore > res.bytesAfter) ? (res.bytesBefore - res.bytesAfter) : 0).c_str(),
                      MB_ICONINFORMATION);
    }
}

void CActConverterDlg::OnBrowseBatchDir() {
    CFolderPickerDialog dlg(nullptr, OFN_FILEMUSTEXIST, this);
    if (dlg.DoModal() == IDOK) {
        m_edtBatchDir.SetWindowTextW(dlg.GetPathName());
    }
}

void CActConverterDlg::OnConvertBatch() {
    CString dirStr;
    m_edtBatchDir.GetWindowTextW(dirStr);
    const std::filesystem::path dir = dirStr.GetString();
    if (dir.empty() || !std::filesystem::exists(dir)) {
        AfxMessageBox(L"Please select a valid directory first.", MB_ICONWARNING);
        return;
    }

    AppendLog(std::format(L"Scanning directory '{}' for version 6 .act files...", dir.wstring()));
    const bool backup = (m_chkBackup.GetCheck() == BST_CHECKED);
    const auto results = Graphics::ActConverter::ConvertDirectoryV6ToV5(dir, true, backup);

    int successCount = 0;
    size_t totalSavedBytes = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        if (r.success) {
            ++successCount;
            const size_t diff = (r.bytesBefore > r.bytesAfter) ? (r.bytesBefore - r.bytesAfter) : 0;
            totalSavedBytes += diff;
            AppendLog(std::format(L"  [OK] {} -> v5 ({} actions, -{} bytes)",
                                  r.filePath.filename().wstring(), r.actionCount, diff));
        } else {
            AppendLog(std::format(L"  [SKIP] {}: {}", r.filePath.filename().wstring(), Utf8ToWide(r.message).GetString()));
        }
    }

    AppendLog(std::format(L"Batch finished: {} version 6 files converted to version 5 (saved {} total bytes).",
                          successCount, totalSavedBytes));

    AfxMessageBox(std::format(L"Batch conversion completed!\nConverted {} files to ACT Version 5.\nTotal bytes saved: {}",
                              successCount, totalSavedBytes).c_str(), MB_ICONINFORMATION);
}

void CActConverterDlg::OnOK() {
    CDialog::OnOK();
}

// ---------------------------------------------------------------------------
// CIconCanvasWnd
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CIconCanvasWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

void CIconCanvasWnd::SetIconBuffer(const Graphics::ImageBuffer& img) {
    if (img.width == 50 && img.height == 50 && img.rgbaData.size() == 50 * 50 * 4) {
        m_icon = img;
    } else if (img.width > 0 && img.height > 0) {
        Graphics::DDSParser::ScaleImageBilinear(img, 50, 50, m_icon);
    } else {
        m_icon.width = 50;
        m_icon.height = 50;
        m_icon.rgbaData.assign(50 * 50 * 4, 0); // Transparent black
    }
    Invalidate(FALSE);
}

void CIconCanvasWnd::SetCurrentColor(COLORREF color, uint8_t alpha) noexcept {
    m_color = color;
    m_alpha = alpha;
}

BOOL CIconCanvasWnd::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE; // All rendering in OnPaint
}

void CIconCanvasWnd::OnPaint() {
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* oldBmp = memDC.SelectObject(&memBmp);

    const int cellW = rc.Width() / 50;
    const int cellH = rc.Height() / 50;

    // Draw background checkerboard pattern for transparency
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            const bool alt = ((x / 2 + y / 2) % 2 == 0);
            const COLORREF bg = alt ? RGB(38, 40, 46) : RGB(28, 30, 34);
            memDC.FillSolidRect(x * cellW, y * cellH, cellW, cellH, bg);
        }
    }

    // Draw 50x50 icon pixels
    if (m_icon.width == 50 && m_icon.height == 50 && m_icon.rgbaData.size() >= 50 * 50 * 4) {
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 50; ++x) {
                const size_t off = (static_cast<size_t>(y) * 50 + x) * 4;
                const uint8_t r = m_icon.rgbaData[off + 0];
                const uint8_t g = m_icon.rgbaData[off + 1];
                const uint8_t b = m_icon.rgbaData[off + 2];
                const uint8_t a = m_icon.rgbaData[off + 3];

                if (a > 0) {
                    if (a == 255) {
                        memDC.FillSolidRect(x * cellW, y * cellH, cellW, cellH, RGB(r, g, b));
                    } else {
                        // Blend against checkerboard
                        const bool alt = ((x / 2 + y / 2) % 2 == 0);
                        const COLORREF bgCol = alt ? RGB(38, 40, 46) : RGB(28, 30, 34);
                        const float af = a / 255.0f;
                        const uint8_t br = static_cast<uint8_t>(r * af + GetRValue(bgCol) * (1.0f - af));
                        const uint8_t bg = static_cast<uint8_t>(g * af + GetGValue(bgCol) * (1.0f - af));
                        const uint8_t bb = static_cast<uint8_t>(b * af + GetBValue(bgCol) * (1.0f - af));
                        memDC.FillSolidRect(x * cellW, y * cellH, cellW, cellH, RGB(br, bg, bb));
                    }
                }
            }
        }
    }

    // Optional pixel grid lines
    if (m_showGrid && cellW >= 4) {
        CPen gridPen(PS_SOLID, 1, RGB(48, 52, 60));
        CPen* oldPen = memDC.SelectObject(&gridPen);
        for (int x = 0; x <= 50; ++x) {
            memDC.MoveTo(x * cellW, 0);
            memDC.LineTo(x * cellW, 50 * cellH);
        }
        for (int y = 0; y <= 50; ++y) {
            memDC.MoveTo(0, y * cellH);
            memDC.LineTo(50 * cellW, y * cellH);
        }
        memDC.SelectObject(oldPen);
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(oldBmp);
}

void CIconCanvasWnd::ApplyToolAt(CPoint pt, bool isRightClick) {
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    const int cellW = rc.Width() / 50;
    const int cellH = rc.Height() / 50;
    if (cellW <= 0 || cellH <= 0) return;

    const int px = std::clamp(static_cast<int>(pt.x / cellW), 0, 49);
    const int py = std::clamp(static_cast<int>(pt.y / cellH), 0, 49);

    if (m_icon.width != 50 || m_icon.height != 50 || m_icon.rgbaData.size() < 50 * 50 * 4) {
        m_icon.width = 50;
        m_icon.height = 50;
        m_icon.rgbaData.assign(50 * 50 * 4, 0);
    }

    const size_t off = (static_cast<size_t>(py) * 50 + px) * 4;

    if (isRightClick || m_tool == Tool::Eraser) {
        m_icon.rgbaData[off + 0] = 0;
        m_icon.rgbaData[off + 1] = 0;
        m_icon.rgbaData[off + 2] = 0;
        m_icon.rgbaData[off + 3] = 0;
    } else if (m_tool == Tool::Pencil) {
        m_icon.rgbaData[off + 0] = GetRValue(m_color);
        m_icon.rgbaData[off + 1] = GetGValue(m_color);
        m_icon.rgbaData[off + 2] = GetBValue(m_color);
        m_icon.rgbaData[off + 3] = m_alpha;
    } else if (m_tool == Tool::Eyedropper) {
        m_color = RGB(m_icon.rgbaData[off + 0], m_icon.rgbaData[off + 1], m_icon.rgbaData[off + 2]);
        m_alpha = m_icon.rgbaData[off + 3];
    } else if (m_tool == Tool::Fill) {
        uint32_t target = 0;
        std::memcpy(&target, &m_icon.rgbaData[off], 4);
        uint32_t fill = (static_cast<uint32_t>(m_alpha) << 24) |
                        (static_cast<uint32_t>(GetBValue(m_color)) << 16) |
                        (static_cast<uint32_t>(GetGValue(m_color)) << 8) |
                        static_cast<uint32_t>(GetRValue(m_color));
        if (target != fill) {
            FloodFill(px, py, target, fill);
        }
    }

    Invalidate(FALSE);
    if (m_onChange) m_onChange();
}

void CIconCanvasWnd::FloodFill(int startX, int startY, uint32_t targetColor, uint32_t fillColor) {
    std::vector<std::pair<int, int>> stack;
    stack.push_back({startX, startY});

    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();

        if (x < 0 || x >= 50 || y < 0 || y >= 50) continue;
        const size_t off = (static_cast<size_t>(y) * 50 + x) * 4;
        uint32_t cur = 0;
        std::memcpy(&cur, &m_icon.rgbaData[off], 4);
        if (cur != targetColor) continue;

        std::memcpy(&m_icon.rgbaData[off], &fillColor, 4);

        stack.push_back({x + 1, y});
        stack.push_back({x - 1, y});
        stack.push_back({x, y + 1});
        stack.push_back({x, y - 1});
    }
}

void CIconCanvasWnd::OnLButtonDown(UINT /*nFlags*/, CPoint point) {
    SetCapture();
    m_drawing = true;
    ApplyToolAt(point, false);
}

void CIconCanvasWnd::OnMouseMove(UINT /*nFlags*/, CPoint point) {
    if (m_drawing && (m_tool == Tool::Pencil || m_tool == Tool::Eraser)) {
        ApplyToolAt(point, false);
    }
}

void CIconCanvasWnd::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/) {
    if (m_drawing) {
        m_drawing = false;
        ::ReleaseCapture();
    }
}

void CIconCanvasWnd::OnRButtonDown(UINT /*nFlags*/, CPoint point) {
    ApplyToolAt(point, true);
}

// ---------------------------------------------------------------------------
// CIconEditorDlg
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CIconEditorDlg, CThemedDialog)
    ON_BN_CLICKED(IDC_ICON_TOOL_PENCIL, &CIconEditorDlg::OnToolPencil)
    ON_BN_CLICKED(IDC_ICON_TOOL_ERASER, &CIconEditorDlg::OnToolEraser)
    ON_BN_CLICKED(IDC_ICON_TOOL_FILL, &CIconEditorDlg::OnToolFill)
    ON_BN_CLICKED(IDC_ICON_TOOL_EYEDROPPER, &CIconEditorDlg::OnToolEyedropper)
    ON_BN_CLICKED(IDC_ICON_TOOL_CLEAR, &CIconEditorDlg::OnToolClear)
    ON_BN_CLICKED(IDC_ICON_GRID_CHK, &CIconEditorDlg::OnToggleGrid)

    ON_COMMAND_RANGE(IDC_ICON_PALETTE_BASE, IDC_ICON_PALETTE_BASE + 15, &CIconEditorDlg::OnColorPaletteBtn)
    ON_CBN_SELCHANGE(IDC_ICON_RANK_COMBO, &CIconEditorDlg::OnRankBorderChanged)
    ON_BN_CLICKED(IDC_ICON_APPLY_BORDER, &CIconEditorDlg::OnRankBorderChanged)

    ON_BN_CLICKED(IDC_ICON_IMPORT_BTN, &CIconEditorDlg::OnImportImage)
    ON_BN_CLICKED(IDC_ICON_PASTE_BTN, &CIconEditorDlg::OnPasteClipboard)
    ON_BN_CLICKED(IDC_ICON_EXP_PNG, &CIconEditorDlg::OnExportPng)
    ON_BN_CLICKED(IDC_ICON_EXP_DDS, &CIconEditorDlg::OnExportDds)
    ON_BN_CLICKED(IDC_ICON_ASSIGN_BTN, &CIconEditorDlg::OnAssignToItem)

    ON_WM_PAINT()
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

CIconEditorDlg::CIconEditorDlg(Engine::WorkspaceEngine& engine, int32_t targetItemId, int32_t initialIconIndex, CWnd* parent)
    : m_tmpl(L"Item Icon Studio & Pixel Creator", 680, 480, WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
      m_engine(engine),
      m_targetItemId(targetItemId),
      m_iconIndex(initialIconIndex) {
    InitModalIndirect(m_tmpl.Get(), parent);
    const auto* it = m_engine.Items().GetItem(m_targetItemId);
    if (it) m_itemRank = it->rank;
}

BOOL CIconEditorDlg::OnInitDialog() {
    CDialog::OnInitDialog();

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fonts.Build(dpi);
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);

    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int clientW = dluX(680);
    const int clientH = dluY(480);
    CRect curClient, curWin;
    GetClientRect(&curClient);
    GetWindowRect(&curWin);
    const int extraW = curWin.Width() - curClient.Width();
    const int extraH = curWin.Height() - curClient.Height();
    SetWindowPos(nullptr, 0, 0, clientW + extraW, clientH + extraH, SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    const auto* it = m_engine.Items().GetItem(m_targetItemId);
    std::wstring hdr = L"Item Icon Studio";
    if (it) {
        hdr = std::format(L"Item Icon Studio: #{} '{}' (Current Icon Index: {})",
                          it->id, Utf8ToWide(it->name).GetString(), m_iconIndex);
    }
    m_lblHeader.Create(hdr.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                       CRect(dluX(10), dluY(8), dluX(670), dluY(22)), this);
    m_lblHeader.SetFont(&m_fonts.bold);

    // Left Pane: Drawing Canvas (50x50 icon scaled to 8x = 400x400 canvas)
    const int canvasSizeDlu = 210;
    m_grpCanvas.Create(L"50x50 Pixel Icon Canvas", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                       CRect(dluX(10), dluY(26), dluX(10 + canvasSizeDlu + 12), dluY(26 + canvasSizeDlu + 32)), this);

    m_canvas.Create(nullptr, L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                    CRect(dluX(16), dluY(42), dluX(16 + canvasSizeDlu), dluY(42 + canvasSizeDlu)), this, IDC_STATIC);
    m_canvas.SetOnChangeCallback([this]() { UpdatePreviews(); });

    m_chkGrid.Create(L"Show Pixel Grid (8x Zoom)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                     CRect(dluX(16), dluY(42 + canvasSizeDlu + 6), dluX(16 + canvasSizeDlu), dluY(42 + canvasSizeDlu + 20)), this, IDC_ICON_GRID_CHK);
    m_chkGrid.SetFont(&m_fonts.regular);
    m_chkGrid.SetCheck(BST_CHECKED);

    // Middle Pane: Tools & Colors
    const int midX = 10 + canvasSizeDlu + 18;
    const int midW = 220;

    m_grpTools.Create(L"Drawing Tools", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      CRect(dluX(midX), dluY(26), dluX(midX + midW), dluY(100)), this);

    const int btnW = 48;
    m_btnToolPencil.Create(L"Pencil", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           CRect(dluX(midX + 8), dluY(42), dluX(midX + 8 + btnW), dluY(62)), this, IDC_ICON_TOOL_PENCIL);
    m_btnToolPencil.SetFont(&m_fonts.regular);

    m_btnToolEraser.Create(L"Eraser", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           CRect(dluX(midX + 60), dluY(42), dluX(midX + 60 + btnW), dluY(62)), this, IDC_ICON_TOOL_ERASER);
    m_btnToolEraser.SetFont(&m_fonts.regular);

    m_btnToolFill.Create(L"Fill", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                         CRect(dluX(midX + 112), dluY(42), dluX(midX + 112 + btnW), dluY(62)), this, IDC_ICON_TOOL_FILL);
    m_btnToolFill.SetFont(&m_fonts.regular);

    m_btnToolEyedropper.Create(L"Pick", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                               CRect(dluX(midX + 164), dluY(42), dluX(midX + 164 + btnW), dluY(62)), this, IDC_ICON_TOOL_EYEDROPPER);
    m_btnToolEyedropper.SetFont(&m_fonts.regular);

    m_btnToolClear.Create(L"Clear Canvas", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(dluX(midX + 8), dluY(68), dluX(midX + midW - 8), dluY(88)), this, IDC_ICON_TOOL_CLEAR);
    m_btnToolClear.SetFont(&m_fonts.regular);

    // Color Palette
    m_grpColors.Create(L"Palette & Preset Rank Colors", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                       CRect(dluX(midX), dluY(106), dluX(midX + midW), dluY(196)), this);

    const COLORREF kColors[16] = {
        RGB(255, 255, 255), RGB(200, 200, 200), RGB(128, 128, 128), RGB(0, 0, 0),
        RGB(50, 205, 50),   RGB(30, 144, 255),  RGB(255, 140, 0),   RGB(186, 85, 211),
        RGB(220, 20, 60),   RGB(255, 215, 0),   RGB(255, 99, 71),   RGB(0, 255, 255),
        RGB(139, 69, 19),   RGB(255, 192, 203), RGB(75, 0, 130),   RGB(255, 255, 0)
    };

    for (int i = 0; i < 16; ++i) {
        const int col = i % 8;
        const int row = i / 8;
        const int bx = midX + 8 + col * 26;
        const int by = 124 + row * 26;
        m_paletteButtons[i].Create(L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                   CRect(dluX(bx), dluY(by), dluX(bx + 22), dluY(by + 22)), this,
                                   IDC_ICON_PALETTE_BASE + i);
    }

    m_lblCurColor.Create(L"Active Color: White (255, 255, 255)", WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                         CRect(dluX(midX + 8), dluY(178), dluX(midX + midW - 8), dluY(190)), this);
    m_lblCurColor.SetFont(&m_fonts.secondary);

    // Rank Border Overlay
    m_grpRankBorder.Create(L"Rank Border Overlay", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                           CRect(dluX(midX), dluY(202), dluX(midX + midW), dluY(268)), this);

    m_lblRankBorder.Create(L"Rank Border:", WS_CHILD | WS_VISIBLE,
                           CRect(dluX(midX + 8), dluY(220), dluX(midX + 70), dluY(232)), this);
    m_lblRankBorder.SetFont(&m_fonts.secondary);

    m_cmbRankBorder.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                           CRect(dluX(midX + 74), dluY(218), dluX(midX + midW - 8), dluY(180)), this, IDC_ICON_RANK_COMBO);
    m_cmbRankBorder.SetFont(&m_fonts.regular);
    m_cmbRankBorder.AddString(L"0 - None (Raw Tile)");
    m_cmbRankBorder.AddString(L"1 - Magic (Green)");
    m_cmbRankBorder.AddString(L"2 - Rare (Blue)");
    m_cmbRankBorder.AddString(L"3 - Epic (Orange)");
    m_cmbRankBorder.AddString(L"4 - Unique (Purple)");
    m_cmbRankBorder.AddString(L"5 - Legend (Red)");
    m_cmbRankBorder.AddString(L"6 - Divine (Gold)");
    m_cmbRankBorder.SetCurSel(std::clamp(m_itemRank, 0, 6));

    m_btnApplyBorder.Create(L"Apply Border to Canvas", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            CRect(dluX(midX + 8), dluY(242), dluX(midX + midW - 8), dluY(260)), this, IDC_ICON_APPLY_BORDER);
    m_btnApplyBorder.SetFont(&m_fonts.regular);

    // Right Pane: Previews & Actions
    const int rightX = midX + midW + 10;

    m_grpPreviews.Create(L"Actual Scale Previews", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                         CRect(dluX(rightX), dluY(26), dluX(670), dluY(130)), this);

    m_lblPreview1x.Create(L"1:1 (50x50):", WS_CHILD | WS_VISIBLE,
                          CRect(dluX(rightX + 8), dluY(42), dluX(rightX + 80), dluY(54)), this);
    m_lblPreview1x.SetFont(&m_fonts.secondary);

    m_lblPreview2x.Create(L"2:1 (100x100):", WS_CHILD | WS_VISIBLE,
                          CRect(dluX(rightX + 80), dluY(42), dluX(660), dluY(54)), this);
    m_lblPreview2x.SetFont(&m_fonts.secondary);

    // Actions Card
    m_grpActions.Create(L"Import, Export & Assign", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        CRect(dluX(rightX), dluY(136), dluX(670), dluY(430)), this);

    m_btnImport.Create(L"Import Image (DDS / BMP)...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(dluX(rightX + 8), dluY(154), dluX(662), dluY(178)), this, IDC_ICON_IMPORT_BTN);
    m_btnImport.SetFont(&m_fonts.regular);

    m_btnPaste.Create(L"Paste from Clipboard", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(rightX + 8), dluY(184), dluX(662), dluY(208)), this, IDC_ICON_PASTE_BTN);
    m_btnPaste.SetFont(&m_fonts.regular);

    m_btnExportPng.Create(L"Export as BMP...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(dluX(rightX + 8), dluY(214), dluX(662), dluY(238)), this, IDC_ICON_EXP_PNG);
    m_btnExportPng.SetFont(&m_fonts.regular);

    m_btnExportDds.Create(L"Export as DDS (RGBA32)...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(dluX(rightX + 8), dluY(244), dluX(662), dluY(268)), this, IDC_ICON_EXP_DDS);
    m_btnExportDds.SetFont(&m_fonts.regular);

    m_btnAssign.Create(L"Assign Icon to Current Item", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(dluX(rightX + 8), dluY(280), dluX(662), dluY(310)), this, IDC_ICON_ASSIGN_BTN);
    m_btnAssign.SetFont(&m_fonts.bold);

    m_btnClose.Create(L"Close Studio", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(dluX(600), dluY(446), dluX(670), dluY(470)), this, IDOK);
    m_btnClose.SetFont(&m_fonts.regular);

    Graphics::ImageBuffer initTile;
    if (m_engine.GetItemIcon(m_iconIndex, initTile)) {
        m_canvas.SetIconBuffer(initTile);
    } else {
        Graphics::ImageBuffer blank;
        blank.width = 50; blank.height = 50;
        blank.rgbaData.assign(50 * 50 * 4, 0);
        m_canvas.SetIconBuffer(blank);
    }

    return TRUE;
}

void CIconEditorDlg::OnToolPencil() { m_canvas.SetCurrentTool(CIconCanvasWnd::Tool::Pencil); }
void CIconEditorDlg::OnToolEraser() { m_canvas.SetCurrentTool(CIconCanvasWnd::Tool::Eraser); }
void CIconEditorDlg::OnToolFill() { m_canvas.SetCurrentTool(CIconCanvasWnd::Tool::Fill); }
void CIconEditorDlg::OnToolEyedropper() { m_canvas.SetCurrentTool(CIconCanvasWnd::Tool::Eyedropper); }

void CIconEditorDlg::OnToolClear() {
    if (AfxMessageBox(L"Clear the entire canvas to transparent?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        Graphics::ImageBuffer blank;
        blank.width = 50; blank.height = 50;
        blank.rgbaData.assign(50 * 50 * 4, 0);
        m_canvas.SetIconBuffer(blank);
        UpdatePreviews();
    }
}

void CIconEditorDlg::OnToggleGrid() {
    m_canvas.SetShowGrid(m_chkGrid.GetCheck() == BST_CHECKED);
}

void CIconEditorDlg::OnColorPaletteBtn(UINT nID) {
    const UINT idx = nID - IDC_ICON_PALETTE_BASE;
    const COLORREF kColors[16] = {
        RGB(255, 255, 255), RGB(200, 200, 200), RGB(128, 128, 128), RGB(0, 0, 0),
        RGB(50, 205, 50),   RGB(30, 144, 255),  RGB(255, 140, 0),   RGB(186, 85, 211),
        RGB(220, 20, 60),   RGB(255, 215, 0),   RGB(255, 99, 71),   RGB(0, 255, 255),
        RGB(139, 69, 19),   RGB(255, 192, 203), RGB(75, 0, 130),   RGB(255, 255, 0)
    };
    if (idx < 16) {
        SetActiveColorUI(kColors[idx]);
    }
}

void CIconEditorDlg::SetActiveColorUI(COLORREF col) {
    m_canvas.SetCurrentColor(col, 255);
    m_canvas.SetCurrentTool(CIconCanvasWnd::Tool::Pencil);
    m_lblCurColor.SetWindowTextW(std::format(L"Active Color: RGB({}, {}, {})",
                                             GetRValue(col), GetGValue(col), GetBValue(col)).c_str());
}

void CIconEditorDlg::OnRankBorderChanged() {
    const int rank = m_cmbRankBorder.GetCurSel();
    if (rank > 0) {
        Graphics::DDSParser::ApplyRankBorder(m_canvas.GetIconBuffer(), rank);
        m_canvas.Invalidate(FALSE);
        UpdatePreviews();
    }
}

void CIconEditorDlg::OnImportImage() {
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    L"Supported Image Files (*.dds;*.bmp)|*.dds;*.bmp|DirectDraw Surface (*.dds)|*.dds|Bitmap (*.bmp)|*.bmp|All Files (*.*)|*.*||", this);
    if (dlg.DoModal() == IDOK) {
        const std::filesystem::path p = dlg.GetPathName().GetString();
        Graphics::ImageBuffer loaded;
        bool ok = false;

        if (_stricmp(p.extension().string().c_str(), ".dds") == 0) {
            ok = Graphics::DDSParser::DecodeFile(p, loaded);
        } else if (_stricmp(p.extension().string().c_str(), ".bmp") == 0) {
            ok = LoadBmp32(p, loaded);
        }

        if (ok) {
            m_canvas.SetIconBuffer(loaded);
            UpdatePreviews();
            AfxMessageBox(std::format(L"Image loaded and rescaled to 50x50 icon format.\nOriginal size: {}x{}",
                                      loaded.width, loaded.height).c_str(), MB_ICONINFORMATION);
        } else {
            AfxMessageBox(L"Failed to decode image file (ensure file is valid 32-bit BMP or DDS).", MB_ICONERROR);
        }
    }
}

void CIconEditorDlg::OnPasteClipboard() {
    Graphics::ImageBuffer loaded;
    if (ReadClipboardBitmap(loaded)) {
        m_canvas.SetIconBuffer(loaded);
        UpdatePreviews();
        AfxMessageBox(std::format(L"Pasted and rescaled image from clipboard into 50x50 icon canvas.\nSource: {}x{}",
                                  loaded.width, loaded.height).c_str(), MB_ICONINFORMATION);
    } else {
        AfxMessageBox(L"No valid bitmap data found on the clipboard.", MB_ICONINFORMATION);
    }
}

void CIconEditorDlg::OnExportPng() {
    CFileDialog dlg(FALSE, L"bmp", L"custom_icon.bmp", OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    L"Bitmap Image (*.bmp)|*.bmp||", this);
    if (dlg.DoModal() == IDOK) {
        const std::filesystem::path p = dlg.GetPathName().GetString();
        if (SaveBmp32(p, m_canvas.GetIconBuffer())) {
            AfxMessageBox(L"Icon successfully saved as 32-bit BMP image.", MB_ICONINFORMATION);
        } else {
            AfxMessageBox(L"Failed to save BMP image.", MB_ICONERROR);
        }
    }
}

void CIconEditorDlg::OnExportDds() {
    CFileDialog dlg(FALSE, L"dds", L"custom_icon.dds", OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                    L"DirectDraw Surface (*.dds)|*.dds||", this);
    if (dlg.DoModal() == IDOK) {
        const std::filesystem::path p = dlg.GetPathName().GetString();
        if (Graphics::DDSParser::SaveFileRGBA32(p, m_canvas.GetIconBuffer())) {
            AfxMessageBox(L"Icon successfully saved as 32-bit RGBA DDS texture.", MB_ICONINFORMATION);
        } else {
            AfxMessageBox(L"Failed to save DDS texture.", MB_ICONERROR);
        }
    }
}

void CIconEditorDlg::OnAssignToItem() {
    const auto& icon = m_canvas.GetIconBuffer();
    if (m_engine.SetCustomItemIcon(m_iconIndex, icon)) {
        AfxMessageBox(std::format(L"Custom icon successfully assigned to Item #{} (Icon Index: {})!\nSaved to workspace custom icon storage.",
                                  m_targetItemId, m_iconIndex).c_str(), MB_ICONINFORMATION);
        EndDialog(IDOK);
    } else {
        AfxMessageBox(L"Failed to assign icon to workspace engine.", MB_ICONERROR);
    }
}

void CIconEditorDlg::UpdatePreviews() {
    Invalidate(FALSE);
}

void CIconEditorDlg::OnPaint() {
    CPaintDC dc(this);

    const auto& icon = m_canvas.GetIconBuffer();
    if (icon.width != 50 || icon.height != 50 || icon.rgbaData.size() < 50 * 50 * 4) {
        return;
    }

    const int dpi = GetWindowDpi(GetSafeHwnd());
    const CSize base = Theme::DialogBaseUnits(this, m_fonts.regular);
    auto dluX = [base](int dlu) { return Theme::DluX(dlu, base); };
    auto dluY = [base](int dlu) { return Theme::DluY(dlu, base); };

    const int midX = 10 + 210 + 18;
    const int rightX = midX + 220 + 10;

    const int p1x = dluX(rightX + 8);
    const int p1y = dluY(56);
    const int p2x = dluX(rightX + 70);
    const int p2y = dluY(56);

    // 1:1 preview (50x50)
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            const size_t off = (static_cast<size_t>(y) * 50 + x) * 4;
            const uint8_t r = icon.rgbaData[off + 0];
            const uint8_t g = icon.rgbaData[off + 1];
            const uint8_t b = icon.rgbaData[off + 2];
            const uint8_t a = icon.rgbaData[off + 3];
            if (a > 0) {
                dc.SetPixel(p1x + x, p1y + y, RGB(r, g, b));
            } else {
                dc.SetPixel(p1x + x, p1y + y, RGB(30, 32, 36));
            }
        }
    }
    CRect rc1(p1x - 1, p1y - 1, p1x + 51, p1y + 51);
    dc.Draw3dRect(&rc1, Theme::kBorder, Theme::kBorder);

    // 2:1 preview (100x100)
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            const size_t off = (static_cast<size_t>(y) * 50 + x) * 4;
            const uint8_t r = icon.rgbaData[off + 0];
            const uint8_t g = icon.rgbaData[off + 1];
            const uint8_t b = icon.rgbaData[off + 2];
            const uint8_t a = icon.rgbaData[off + 3];
            const COLORREF col = (a > 0) ? RGB(r, g, b) : RGB(30, 32, 36);
            dc.FillSolidRect(p2x + x * 2, p2y + y * 2, 2, 2, col);
        }
    }
    CRect rc2(p2x - 1, p2y - 1, p2x + 101, p2y + 101);
    dc.Draw3dRect(&rc2, Theme::kBorder, Theme::kBorder);
}

void CIconEditorDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS) {
    if (lpDIS && lpDIS->CtlType == ODT_BUTTON &&
        lpDIS->CtlID >= IDC_ICON_PALETTE_BASE &&
        lpDIS->CtlID < IDC_ICON_PALETTE_BASE + 16) {
        const UINT idx = lpDIS->CtlID - IDC_ICON_PALETTE_BASE;
        const COLORREF kColors[16] = {
            RGB(255, 255, 255), RGB(200, 200, 200), RGB(128, 128, 128), RGB(0, 0, 0),
            RGB(50, 205, 50),   RGB(30, 144, 255),  RGB(255, 140, 0),   RGB(186, 85, 211),
            RGB(220, 20, 60),   RGB(255, 215, 0),   RGB(255, 99, 71),   RGB(0, 255, 255),
            RGB(139, 69, 19),   RGB(255, 192, 203), RGB(75, 0, 130),   RGB(255, 255, 0)
        };
        CDC dc;
        dc.Attach(lpDIS->hDC);
        CRect rc = lpDIS->rcItem;
        dc.FillSolidRect(&rc, kColors[idx]);
        if (lpDIS->itemState & ODS_SELECTED) {
            dc.Draw3dRect(&rc, RGB(255, 255, 255), RGB(255, 255, 255));
            rc.DeflateRect(1, 1);
            dc.Draw3dRect(&rc, RGB(0, 0, 0), RGB(0, 0, 0));
        } else {
            dc.Draw3dRect(&rc, Theme::kBorder, Theme::kBorder);
        }
        dc.Detach();
        return;
    }
    CDialog::OnDrawItem(nIDCtl, lpDIS);
}

void CIconEditorDlg::OnOK() {
    CDialog::OnOK();
}

} // namespace DNItemMaker::UI
