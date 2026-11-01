#include "UI/FlatControls.h"

#include "UI/Theme.h"



#include <uxtheme.h>



namespace DNItemMaker::UI {



// ---------------------------------------------------------------------------

// CFlatEdit

// ---------------------------------------------------------------------------



BEGIN_MESSAGE_MAP(CFlatEdit, CEdit)

    ON_WM_NCPAINT()

    ON_WM_PAINT()

    ON_WM_SETFOCUS()

    ON_WM_KILLFOCUS()

    ON_WM_ENABLE()

END_MESSAGE_MAP()



void CFlatEdit::PreSubclassWindow() {

    CEdit::PreSubclassWindow();

    // The visual-styles engine repaints a themed edit's border after WM_NCPAINT, covering ours.

    // Single-line edits have nothing else themed, so opt them out; scrolling edits keep the theme.

    if ((GetStyle() & (WS_VSCROLL | WS_HSCROLL)) == 0) {

        ::SetWindowTheme(GetSafeHwnd(), L"", L"");

    }

}



void CFlatEdit::OnNcPaint() {

    Default(); // scroll bars of multi-line edits

    DrawFlatBorder();

}



void CFlatEdit::OnPaint() {

    Default();

}



void CFlatEdit::DrawFlatBorder() {

    // The edit control strips WS_BORDER from its style at creation. A ComCtl v6 edit then draws

    // the border on the outer pixel of its client area; edits that keep a non-client inset

    // (scroll bars) draw it there instead. Cover whichever one it used.

    COLORREF color = Theme::kBorder;

    if (::GetFocus() == GetSafeHwnd() && (GetStyle() & ES_READONLY) == 0) {

        color = Theme::kAccent;

    }



    CRect windowRc;

    GetWindowRect(&windowRc);

    CPoint clientOrigin(0, 0);

    ClientToScreen(&clientOrigin);

    CRect clientRc;

    GetClientRect(&clientRc);

    const bool borderInClient = clientOrigin.x == windowRc.left && clientOrigin.y == windowRc.top;



    if (borderInClient) {

        CClientDC dc(this);

        Theme::FrameRect1px(dc, clientRc, color);

    }

    if (!borderInClient || clientRc.Width() != windowRc.Width() || clientRc.Height() != windowRc.Height()) {

        // Non-client border, or scroll bars outside the client: frame the whole window so they sit inside.

        CWindowDC dc(this);

        windowRc.OffsetRect(-windowRc.left, -windowRc.top);

        Theme::FrameRect1px(dc, windowRc, color);

    }

}



void CFlatEdit::OnSetFocus(CWnd* pOldWnd) {

    CEdit::OnSetFocus(pOldWnd);

    DrawFlatBorder();

}



void CFlatEdit::OnKillFocus(CWnd* pNewWnd) {

    CEdit::OnKillFocus(pNewWnd);

    DrawFlatBorder();

}



void CFlatEdit::OnEnable(BOOL bEnable) {

    CEdit::OnEnable(bEnable);

    RedrawWindow(nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);

}



// ---------------------------------------------------------------------------

// CFlatButton

// ---------------------------------------------------------------------------



BEGIN_MESSAGE_MAP(CFlatButton, CButton)

    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CFlatButton::OnCustomDraw)

END_MESSAGE_MAP()



void CFlatButton::SetPrimary(bool primary) {

    m_primary = primary;

    if (GetSafeHwnd()) Invalidate(FALSE);

}



void CFlatButton::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {

    auto* cd = reinterpret_cast<NMCUSTOMDRAW*>(pNMHDR);

    *pResult = CDRF_DODEFAULT;



    const UINT type = GetButtonStyle() & BS_TYPEMASK;

    if (cd->dwDrawStage != CDDS_PREPAINT || (type != BS_PUSHBUTTON && type != BS_DEFPUSHBUTTON)) return;



    const UINT state = cd->uItemState;

    const bool disabled = (state & CDIS_DISABLED) != 0;

    const bool pressed = (state & CDIS_SELECTED) != 0;

    const bool hot = (state & CDIS_HOT) != 0;

    const bool primary = m_primary || type == BS_DEFPUSHBUTTON;



    CDC* dc = CDC::FromHandle(cd->hdc);

    CRect rc(cd->rc);



    COLORREF fill = Theme::kSurface;

    COLORREF edge = Theme::kBorder;

    if (disabled) {

        fill = Theme::kChrome;

    } else if (pressed) {

        fill = Theme::kAccentSoft;

        edge = Theme::kAccent;

    } else if (hot) {

        fill = RGB(236, 241, 246);

        edge = primary ? Theme::kAccent : Theme::kBorderHot;

    } else if (primary) {

        edge = Theme::kAccent;

    }

    dc->FillSolidRect(&rc, fill);

    Theme::FrameRect1px(*dc, rc, edge);



    if ((state & CDIS_FOCUS) && (state & CDIS_SHOWKEYBOARDCUES)) {

        CRect focus = rc;

        focus.DeflateRect(3, 3);

        dc->SetTextColor(Theme::kText);

        dc->SetBkColor(fill);

        dc->DrawFocusRect(&focus);

    }



    CString text;

    GetWindowText(text);

    CRect textRc = rc;

    textRc.DeflateRect(4, 0);

    CFont* oldFont = dc->SelectObject(GetFont());

    dc->SetBkMode(TRANSPARENT);

    dc->SetTextColor(disabled ? Theme::kTextMuted : Theme::kText);

    UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;

    if ((state & CDIS_SHOWKEYBOARDCUES) == 0) format |= DT_HIDEPREFIX;

    dc->DrawText(text, &textRc, format);

    dc->SelectObject(oldFont);



    *pResult = CDRF_SKIPDEFAULT;

}



// ---------------------------------------------------------------------------

// CFlatGroupBox

// ---------------------------------------------------------------------------



BEGIN_MESSAGE_MAP(CFlatGroupBox, CButton)

    ON_WM_ERASEBKGND()

    ON_WM_PAINT()

    ON_WM_NCHITTEST()

END_MESSAGE_MAP()



BOOL CFlatGroupBox::Create(LPCTSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {

    return CButton::Create(lpszCaption, (dwStyle | WS_CHILD) | WS_CLIPSIBLINGS, rect, pParentWnd, nID);

}



void CFlatGroupBox::PreSubclassWindow() {

    CButton::PreSubclassWindow();

    ModifyStyle(0, WS_CLIPSIBLINGS);

    ::SetWindowTheme(GetSafeHwnd(), L"", L"");

}



BOOL CFlatGroupBox::OnEraseBkgnd(CDC* pDC) {
    if (!pDC) return TRUE;
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return TRUE;

    // Exclude all visible sibling controls within this group box
    // so background filling never paints over any sibling buttons, labels, edits, etc.
    CWnd* pParent = GetParent();
    if (pParent && pParent->GetSafeHwnd()) {
        CRect rcScreen = rc;
        ClientToScreen(&rcScreen);

        wchar_t className[64];
        for (CWnd* pSibling = pParent->GetWindow(GW_CHILD); pSibling != nullptr; pSibling = pSibling->GetWindow(GW_HWNDNEXT)) {
            if (pSibling == this || !pSibling->GetSafeHwnd() || !pSibling->IsWindowVisible()) {
                continue;
            }
            // Skip other group boxes so they can handle their own backgrounds
            className[0] = 0;
            ::GetClassNameW(pSibling->GetSafeHwnd(), className, 64);
            if (_wcsicmp(className, L"Button") == 0) {
                const LONG style = ::GetWindowLongW(pSibling->GetSafeHwnd(), GWL_STYLE);
                if ((style & BS_TYPEMASK) == BS_GROUPBOX) {
                    continue;
                }
            }

            CRect rcSib;
            pSibling->GetWindowRect(&rcSib);
            CRect rcIntersect;
            if (rcIntersect.IntersectRect(&rcScreen, &rcSib)) {
                ScreenToClient(&rcIntersect);
                pDC->ExcludeClipRect(&rcIntersect);
            }
        }
    }

    pDC->FillSolidRect(&rc, Theme::kChrome);
    return TRUE;
}



LRESULT CFlatGroupBox::OnNcHitTest(CPoint /*point*/) {

    return HTTRANSPARENT;

}



void CFlatGroupBox::OnPaint() {

    CPaintDC dc(this);

    CRect rc;

    GetClientRect(&rc);

    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    // Do NOT fill the interior rect: sibling controls inside the groupbox would be erased.
    // Background is cleanly painted by the parent window.

    CString text;

    GetWindowText(text);



    CFont* oldFont = dc.SelectObject(GetFont());

    dc.SetBkMode(TRANSPARENT);



    if (text.IsEmpty()) {

        Theme::FrameRect1px(dc, rc, Theme::kBorder);

    } else {

        CSize textSize = dc.GetTextExtent(text);

        const int pad = 4;

        const int textLeft = rc.left + 8;

        const int textRight = textLeft + textSize.cx + pad * 2;

        const int lineY = rc.top + std::max<int>(6, static_cast<int>(textSize.cy / 2));



        // Clear only behind the text caption so the border line doesn't cut through the text

        CRect textBgRc(textLeft, rc.top, textRight, rc.top + textSize.cy);

        dc.FillSolidRect(&textBgRc, Theme::kChrome);



        // Top line left segment

        if (textLeft > rc.left) {

            dc.FillSolidRect(rc.left, lineY, textLeft - rc.left, 1, Theme::kBorder);

        }

        // Top line right segment

        if (rc.right > textRight) {

            dc.FillSolidRect(textRight, lineY, rc.right - textRight, 1, Theme::kBorder);

        }

        // Left, right, bottom lines

        dc.FillSolidRect(rc.left, lineY, 1, rc.bottom - lineY, Theme::kBorder);

        dc.FillSolidRect(rc.right - 1, lineY, 1, rc.bottom - lineY, Theme::kBorder);

        dc.FillSolidRect(rc.left, rc.bottom - 1, rc.Width(), 1, Theme::kBorder);



        // Text label

        CRect textRc(textLeft + pad, rc.top, textRight - pad, rc.top + textSize.cy);

        dc.SetTextColor(Theme::kText);

        dc.DrawText(text, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    }



    dc.SelectObject(oldFont);

}



// ---------------------------------------------------------------------------

// CFlatTabCtrl

// ---------------------------------------------------------------------------



BEGIN_MESSAGE_MAP(CFlatTabCtrl, CTabCtrl)

    ON_WM_ERASEBKGND()

    ON_WM_PAINT()

END_MESSAGE_MAP()



BOOL CFlatTabCtrl::OnEraseBkgnd(CDC* /*pDC*/) {

    return TRUE;

}



void CFlatTabCtrl::OnPaint() {

    CPaintDC paintDC(this);

    CRect client;

    GetClientRect(&client);

    if (client.Width() <= 0 || client.Height() <= 0) return;



    CDC memDC;

    memDC.CreateCompatibleDC(&paintDC);

    CBitmap bmp;

    bmp.CreateCompatibleBitmap(&paintDC, client.Width(), client.Height());

    CBitmap* oldBmp = memDC.SelectObject(&bmp);



    memDC.FillSolidRect(&client, Theme::kChrome);



    const int count = GetItemCount();

    const int sel = GetCurSel();

    int rowBottom = client.top;

    for (int i = 0; i < count; ++i) {

        CRect itemRc;

        if (GetItemRect(i, &itemRc)) rowBottom = std::max(rowBottom, static_cast<int>(itemRc.bottom));

    }



    // Pane outline below the tab row.

    Theme::FrameRect1px(memDC, CRect(client.left, rowBottom, client.right, client.bottom), Theme::kBorder);



    CFont* oldFont = memDC.SelectObject(GetFont());

    memDC.SetBkMode(TRANSPARENT);

    const bool showFocus = ::GetFocus() == GetSafeHwnd() &&

                           (SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS) == 0;



    for (int i = 0; i < count; ++i) {

        CRect itemRc;

        if (!GetItemRect(i, &itemRc)) continue;

        itemRc.bottom = rowBottom;



        wchar_t label[128]{};

        TCITEMW item{};

        item.mask = TCIF_TEXT;

        item.pszText = label;

        item.cchTextMax = static_cast<int>(std::size(label));

        GetItem(i, &item);



        if (i == sel) {

            // Selected tab: open at the bottom so it merges with the pane, accent bar on top.

            CRect tabRc = itemRc;

            tabRc.top = std::max(client.top, tabRc.top - 2);

            memDC.FillSolidRect(tabRc.left, tabRc.top, tabRc.Width(), tabRc.Height() + 1, Theme::kChrome);

            memDC.FillSolidRect(tabRc.left, tabRc.top, 1, tabRc.Height() + 1, Theme::kBorder);

            memDC.FillSolidRect(tabRc.right - 1, tabRc.top, 1, tabRc.Height() + 1, Theme::kBorder);

            memDC.FillSolidRect(tabRc.left, tabRc.top, tabRc.Width(), 2, Theme::kAccent);

            memDC.SetTextColor(Theme::kText);

            itemRc = tabRc;

            itemRc.top += 2;

        } else {

            memDC.SetTextColor(Theme::kTextMuted);

        }



        memDC.DrawText(label, -1, &itemRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX);



        if (i == sel && showFocus) {

            CRect focus = itemRc;

            focus.DeflateRect(3, 2);

            memDC.SetTextColor(Theme::kText);

            memDC.SetBkColor(Theme::kChrome);

            memDC.DrawFocusRect(&focus);

        }

    }



    memDC.SelectObject(oldFont);







    paintDC.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(oldBmp);

}



// ---------------------------------------------------------------------------

// Stock child restyling

// ---------------------------------------------------------------------------



void ApplyFlatChildStyles(CWnd* parent) {

    for (CWnd* child = parent->GetWindow(GW_CHILD); child; child = child->GetNextWindow()) {

        wchar_t cls[32]{};

        ::GetClassNameW(child->GetSafeHwnd(), cls, static_cast<int>(std::size(cls)));

        if (::_wcsicmp(cls, WC_COMBOBOXW) == 0 && (child->GetStyle() & 0x3) == CBS_DROPDOWNLIST) {

            // "CFD" (common file dialog) renders drop-down lists as a white field instead of a gray button.

            ::SetWindowTheme(child->GetSafeHwnd(), L"CFD", nullptr);

        }

    }

}



} // namespace DNItemMaker::UI

