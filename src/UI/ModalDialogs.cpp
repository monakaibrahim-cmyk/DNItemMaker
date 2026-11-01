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
    : m_tmpl(L"Dragon Jewel & Socket Studio", 520, 360),
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

    const int clientW = dluX(525);
    const int clientH = dluY(360);
    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
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
    : m_tmpl(L"Set Item Bonus Inspector & Simulator", 540, 360),
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

    const int clientW = dluX(545);
    const int clientH = dluY(365);
    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
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
    : m_tmpl(L"Enchantment Curve & Odds Simulator", 540, 360),
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

    const int clientW = dluX(545);
    const int clientH = dluY(365);
    CRect winRc(0, 0, clientW, clientH);
    ::AdjustWindowRectEx(&winRc, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, winRc.Width(), winRc.Height(), SWP_NOMOVE | SWP_NOZORDER);
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

} // namespace DNItemMaker::UI
