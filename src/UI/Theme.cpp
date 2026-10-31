#include "UI/Theme.h"

namespace DNItemMaker::UI::Theme {

namespace {

bool IsEditControl(CWnd* wnd) {
    wchar_t cls[16]{};
    return wnd && ::GetClassNameW(wnd->GetSafeHwnd(), cls, 16) > 0 && ::_wcsicmp(cls, L"Edit") == 0;
}

void CreateUiFont(CFont& font, int dpi, int points10, int weight, const wchar_t* face, BYTE pitchAndFamily) {
    font.DeleteObject();
    LOGFONTW lf{};
    lf.lfHeight = -::MulDiv(points10, dpi, 720);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = pitchAndFamily;
    wcscpy_s(lf.lfFaceName, face);
    font.CreateFontIndirectW(&lf);
}

} // namespace

HBRUSH ChromeBrush() {
    static CBrush s_brush(kChrome);
    return s_brush;
}

HBRUSH SurfaceBrush() {
    static CBrush s_brush(kSurface);
    return s_brush;
}

void FontSet::Build(int dpi) {
    constexpr BYTE kSans = VARIABLE_PITCH | FF_SWISS;
    CreateUiFont(regular, dpi, 90, FW_NORMAL, L"Segoe UI", kSans);
    CreateUiFont(bold, dpi, 90, FW_SEMIBOLD, L"Segoe UI", kSans);
    CreateUiFont(secondary, dpi, 80, FW_NORMAL, L"Segoe UI", kSans);
    CreateUiFont(title, dpi, 110, FW_SEMIBOLD, L"Segoe UI", kSans);
    CreateUiFont(mono, dpi, 90, FW_NORMAL, L"Consolas", FIXED_PITCH | FF_MODERN);
}

CSize DialogBaseUnits(CWnd* wnd, CFont& font) {
    CClientDC dc(wnd);
    CFont* old = dc.SelectObject(&font);
    TEXTMETRICW tm{};
    dc.GetTextMetrics(&tm);
    const CSize ext = dc.GetTextExtent(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52);
    dc.SelectObject(old);
    return CSize((ext.cx / 26 + 1) / 2, tm.tmHeight);
}

HBRUSH CtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    switch (nCtlColor) {
        case CTLCOLOR_DLG:
        case CTLCOLOR_BTN: // pixels around push-button corners
            return ChromeBrush();
        case CTLCOLOR_STATIC: // labels, group boxes, check boxes, read-only and disabled edits
            pDC->SetTextColor(kText);
            if (IsEditControl(pWnd)) {
                // Opaque: transparent text inside an edit smears when it scrolls.
                pDC->SetBkColor(kChrome);
            } else {
                pDC->SetBkMode(TRANSPARENT);
            }
            return ChromeBrush();
        case CTLCOLOR_EDIT:
        case CTLCOLOR_LISTBOX: // includes combo box drop-down lists
            pDC->SetTextColor(kText);
            pDC->SetBkColor(kSurface);
            return SurfaceBrush();
        default:
            return nullptr;
    }
}

} // namespace DNItemMaker::UI::Theme
