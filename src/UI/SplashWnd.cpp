#include "UI/SplashWnd.h"
#include "version.h"

#include <format>

namespace DNItemMaker::UI {

BEGIN_MESSAGE_MAP(CSplashWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(WM_APP_SPLASH_PROGRESS, &CSplashWnd::OnSplashProgress)
END_MESSAGE_MAP()

CSplashWnd::CSplashWnd() = default;

CSplashWnd::~CSplashWnd() {
    CloseSplash();
}

bool CSplashWnd::CreateAndShow() {
    m_mainThreadId = ::GetCurrentThreadId();

    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW,
                                      ::LoadCursor(nullptr, IDC_ARROW),
                                      nullptr, nullptr);

    const int dpi = GetWindowDpi(nullptr);
    const int w = DpiScale(580, dpi);
    const int h = DpiScale(310, dpi);
    const int scrW = ::GetSystemMetrics(SM_CXSCREEN);
    const int scrH = ::GetSystemMetrics(SM_CYSCREEN);
    const int x = std::max(0, (scrW - w) / 2);
    const int y = std::max(0, (scrH - h) / 2);

    if (!CreateEx(WS_EX_TOPMOST, cls, L"DNItemMaker Loading",
                  WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
                  CRect(x, y, x + w, y + h), nullptr, 0)) {
        return false;
    }

    m_hIcon = AfxGetApp()->LoadIconW(IDI_APP_ICON);

    m_fontTitle.CreateFontW(-DpiScale(19, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontSubtitle.CreateFontW(-DpiScale(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontStage.CreateFontW(-DpiScale(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, 0,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontDetail.CreateFontW(-DpiScale(9, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontMeta.CreateFontW(-DpiScale(8, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    ShowWindow(SW_SHOW);
    UpdateWindow();
    PumpMessages();
    return true;
}

void CSplashWnd::SetProgress(float progress, std::string_view stage, std::string_view detail) {
    {
        std::lock_guard lock(m_mutex);
        m_progress = std::clamp(progress, 0.0f, 1.0f);
        if (!stage.empty()) m_stageTitle = stage;
        if (!detail.empty()) m_detailText = detail;
    }

    HWND hwnd = GetSafeHwnd();
    if (!hwnd || !::IsWindow(hwnd)) return;

    if (::GetCurrentThreadId() == m_mainThreadId) {
        Invalidate(FALSE);
        UpdateWindow();
        PumpMessages();
    } else {
        // Asynchronously post to main UI thread; never blocks the calling worker thread!
        ::PostMessageW(hwnd, WM_APP_SPLASH_PROGRESS, 0, 0);
    }
}

LRESULT CSplashWnd::OnSplashProgress(WPARAM /*wParam*/, LPARAM /*lParam*/) {
    if (GetSafeHwnd()) {
        Invalidate(FALSE);
        UpdateWindow();
    }
    return 0;
}

void CSplashWnd::CloseSplash() {
    if (GetSafeHwnd()) {
        DestroyWindow();
    }
}

BOOL CSplashWnd::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE; // All painting is handled double-buffered in OnPaint
}

void CSplashWnd::OnPaint() {
    CPaintDC paintDC(this);
    CRect client;
    GetClientRect(&client);
    if (client.Width() <= 0 || client.Height() <= 0) return;

    float progress;
    std::string stageTitle;
    std::string detailText;
    {
        std::lock_guard lock(m_mutex);
        progress = m_progress;
        stageTitle = m_stageTitle;
        detailText = m_detailText;
    }

    CDC memDC;
    memDC.CreateCompatibleDC(&paintDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paintDC, client.Width(), client.Height());
    CBitmap* oldBmp = memDC.SelectObject(&bmp);

    const int dpi = GetWindowDpi(GetSafeHwnd());
    const COLORREF bgDark = RGB(22, 26, 36);
    const COLORREF borderClr = RGB(55, 68, 92);
    const COLORREF sepClr = RGB(36, 44, 62);

    memDC.FillSolidRect(&client, bgDark);
    memDC.Draw3dRect(&client, borderClr, borderClr);
    memDC.SetBkMode(TRANSPARENT);

    // 1. App Icon (48x48)
    const int pad = DpiScale(22, dpi);
    const int iconSz = DpiScale(48, dpi);
    if (m_hIcon) {
        ::DrawIconEx(memDC.GetSafeHdc(), pad, pad, m_hIcon, iconSz, iconSz, 0, nullptr, DI_NORMAL);
    }

    // 2. Header Branding
    const int textX = pad + iconSz + DpiScale(16, dpi);
    CRect rcTitle(textX, pad, client.right - pad, pad + DpiScale(24, dpi));
    CFont* oldFont = memDC.SelectObject(&m_fontTitle);
    memDC.SetTextColor(RGB(245, 248, 255));
    memDC.DrawTextW(L"DNItemMaker", &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    CRect rcSub(textX, rcTitle.bottom + DpiScale(2, dpi), client.right - pad, rcTitle.bottom + DpiScale(18, dpi));
    memDC.SelectObject(&m_fontSubtitle);
    memDC.SetTextColor(RGB(150, 170, 210));
    memDC.DrawTextW(L"Dragon Nest Item & Resource Studio", &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    CRect rcVer(textX, rcSub.bottom + DpiScale(2, dpi), client.right - pad, rcSub.bottom + DpiScale(16, dpi));
    memDC.SelectObject(&m_fontMeta);
    memDC.SetTextColor(RGB(105, 125, 160));
    const CString verText = Utf8ToWide(std::format("v{} (Release x64) • ISO C++23 High-Performance Architecture",
                                                  Resources::Version::ShortVersionString));
    memDC.DrawTextW(verText, &rcVer, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    // 3. Separator Hairline
    const int sepY = pad + iconSz + DpiScale(18, dpi);
    memDC.FillSolidRect(pad, sepY, client.Width() - pad * 2, 1, sepClr);

    // 4. Real-time Loading Stage & Detail Activity
    const int stageY = sepY + DpiScale(18, dpi);
    CRect rcStage(pad, stageY, client.right - pad, stageY + DpiScale(20, dpi));
    memDC.SelectObject(&m_fontStage);
    memDC.SetTextColor(RGB(225, 235, 255));
    memDC.DrawTextW(Utf8ToWide(stageTitle), &rcStage, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    CRect rcDetail(pad, rcStage.bottom + DpiScale(3, dpi), client.right - pad, rcStage.bottom + DpiScale(18, dpi));
    memDC.SelectObject(&m_fontDetail);
    memDC.SetTextColor(RGB(160, 180, 215));
    memDC.DrawTextW(Utf8ToWide(detailText), &rcDetail, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // 5. Modern Double-Buffered Progress Bar
    const int barY = rcDetail.bottom + DpiScale(14, dpi);
    const int barW = client.Width() - pad * 2;
    const int barH = DpiScale(16, dpi);
    const CRect barRc(pad, barY, pad + barW, barY + barH);

    const COLORREF barBg = RGB(12, 15, 22);
    const COLORREF barBorder = RGB(45, 56, 78);
    const COLORREF barFill = RGB(45, 140, 240);

    memDC.FillSolidRect(&barRc, barBg);
    memDC.Draw3dRect(&barRc, barBorder, barBorder);

    const int fillW = static_cast<int>(static_cast<float>(barW - 2) * progress);
    if (fillW > 0) {
        CRect fillRc(barRc.left + 1, barRc.top + 1, barRc.left + 1 + fillW, barRc.bottom - 1);
        memDC.FillSolidRect(&fillRc, barFill);
    }

    // Progress percentage
    const int pct = static_cast<int>(progress * 100.0f);
    const CString pctStr = Utf8ToWide(std::format("{}%", pct));
    CRect pctRc = barRc;
    pctRc.DeflateRect(DpiScale(6, dpi), 0);
    memDC.SelectObject(&m_fontMeta);
    memDC.SetTextColor(RGB(240, 245, 255));
    memDC.DrawTextW(pctStr, &pctRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 6. Bottom Footer Ticker
    const int footerY = client.bottom - DpiScale(26, dpi);
    CRect rcFootLeft(pad, footerY, client.right / 2, client.bottom - DpiScale(8, dpi));
    memDC.SetTextColor(RGB(100, 120, 150));
    memDC.DrawTextW(L"In-Memory Native Cache \x2022 Zero External Daemons", &rcFootLeft, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect rcFootRight(client.right / 2, footerY, client.right - pad, client.bottom - DpiScale(8, dpi));
    memDC.DrawTextW(L"Dragon Nest Eternity Engine", &rcFootRight, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    memDC.SelectObject(oldFont);

    paintDC.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(oldBmp);
}

void CSplashWnd::PumpMessages() {
    if (::GetCurrentThreadId() != m_mainThreadId) return;
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

} // namespace DNItemMaker::UI
