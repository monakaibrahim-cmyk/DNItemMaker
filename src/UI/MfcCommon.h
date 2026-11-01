#pragma once

#ifndef WINVER
    #define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
#endif
#ifndef VC_EXTRALEAN
    #define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

// MFC core headers MUST precede any Win32 or standard library headers that might pull in <Windows.h>.
#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include <afxdlgs.h>

#include <shobjidl.h>
#include <shellapi.h>

// Enable ComCtl32 v6 modern Windows visual styles.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "Graphics/DDSParser.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace DNItemMaker::UI {

inline CString Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return CString();
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return CString();
    CString out;
    wchar_t* buf = out.GetBuffer(needed);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), buf, needed);
    out.ReleaseBuffer(needed);
    return out;
}

inline std::string WideToUtf8(const CString& wide) {
    if (wide.IsEmpty()) return {};
    const int len = wide.GetLength();
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide.GetString(), len, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.GetString(), len, out.data(), needed, nullptr, nullptr);
    return out;
}

inline int GetWindowDpi(HWND hwnd) noexcept {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static const GetDpiForWindowFn s_getDpiForWindow = []() noexcept -> GetDpiForWindowFn {
        if (HMODULE user32 = ::GetModuleHandleW(L"user32.dll")) {
            return reinterpret_cast<GetDpiForWindowFn>(::GetProcAddress(user32, "GetDpiForWindow"));
        }
        return nullptr;
    }();

    if (hwnd && s_getDpiForWindow) {
        const UINT dpi = s_getDpiForWindow(hwnd);
        if (dpi > 0) return static_cast<int>(dpi);
    }

    static const int s_systemDpi = []() noexcept -> int {
        HDC screen = ::GetDC(nullptr);
        const int dpi = screen ? ::GetDeviceCaps(screen, LOGPIXELSY) : 96;
        if (screen) ::ReleaseDC(nullptr, screen);
        return dpi > 0 ? dpi : 96;
    }();
    return s_systemDpi;
}

inline int DpiScale(int px, int dpi) noexcept {
    return ::MulDiv(px, dpi, 96);
}

/// Authentic Dragon Nest item rank color (from client itemcolor:: namespace in EtUITextUtil.h).
inline COLORREF RankTextColor(int32_t rank, bool onDarkBg = false) noexcept {
    switch (rank) {
        case 0: return onDarkBg ? RGB(225, 228, 232) : RGB(45, 50, 58);    // Normal (D) - itemcolor::NORMAL 0xFFFFFFFF
        case 1: return onDarkBg ? RGB(136, 171, 7)   : RGB(88, 130, 5);     // Magic (C) - itemcolor::MAGIC 0xFF88AB07
        case 2: return onDarkBg ? RGB(121, 146, 254) : RGB(35, 85, 210);    // Rare (B) - itemcolor::RARE 0xFF7992FE
        case 3: return onDarkBg ? RGB(255, 145, 72)  : RGB(215, 95, 10);    // Epic (A) - itemcolor::EPIC 0xFFFF9148
        case 4: return onDarkBg ? RGB(191, 139, 255) : RGB(140, 55, 220);   // Unique (S) - itemcolor::UNIQUE 0xFFBF8BFF
        case 5: return onDarkBg ? RGB(220, 35, 85)   : RGB(195, 20, 60);    // Legend / Heroic (SS) - itemcolor::HEROIC 0xFFDC2355
        case 6: return onDarkBg ? RGB(255, 210, 50)  : RGB(185, 140, 5);    // Named / Ancient / Divine (SSS) - itemcolor::NAMED 0xFFFFD232
        default: return onDarkBg ? RGB(200, 200, 200) : RGB(60, 60, 60);
    }
}

// Alpha-composited blit from ImageBuffer to a target DC.
inline void BlitImageBuffer(HDC hdc, const CRect& dstRect, const Graphics::ImageBuffer& img,
                            COLORREF bgColor = RGB(26, 28, 36), bool smoothScale = false) {
    if (!hdc || img.width == 0 || img.height == 0 || img.rgbaData.size() < static_cast<size_t>(img.width) * img.height * 4) {
        return;
    }
    const int dstW = dstRect.Width();
    const int dstH = dstRect.Height();
    if (dstW <= 0 || dstH <= 0) return;

    const uint8_t bgR = GetRValue(bgColor);
    const uint8_t bgG = GetGValue(bgColor);
    const uint8_t bgB = GetBValue(bgColor);

    const size_t pixelCount = static_cast<size_t>(img.width) * img.height;
    thread_local std::vector<uint8_t> s_bgraScratch;
    s_bgraScratch.resize(pixelCount * 4);

    auto div255 = [](uint32_t v) noexcept -> uint8_t {
        const uint32_t t = v + 128u;
        return static_cast<uint8_t>((t + (t >> 8)) >> 8);
    };

    const uint8_t* src = img.rgbaData.data();
    uint8_t* dst = s_bgraScratch.data();
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* sp = src + i * 4;
        uint8_t* dp = dst + i * 4;
        const uint32_t a = sp[3];
        if (a == 255u) {
            dp[0] = sp[2];
            dp[1] = sp[1];
            dp[2] = sp[0];
            dp[3] = 255u;
        } else if (a == 0u) {
            dp[0] = bgB;
            dp[1] = bgG;
            dp[2] = bgR;
            dp[3] = 255u;
        } else {
            const uint32_t invA = 255u - a;
            dp[0] = div255(sp[2] * a + bgB * invA);
            dp[1] = div255(sp[1] * a + bgG * invA);
            dp[2] = div255(sp[0] * a + bgR * invA);
            dp[3] = 255u;
        }
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(img.width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(img.height); // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int oldMode = ::SetStretchBltMode(hdc, smoothScale ? HALFTONE : COLORONCOLOR);
    if (smoothScale) ::SetBrushOrgEx(hdc, 0, 0, nullptr);
    ::StretchDIBits(hdc,
                    dstRect.left, dstRect.top, dstW, dstH,
                    0, 0, static_cast<int>(img.width), static_cast<int>(img.height),
                    s_bgraScratch.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    ::SetStretchBltMode(hdc, oldMode);
}

// Helper structure for programmatic modal dialog templates (InitModalIndirect).
struct EmptyDialogTemplate {
    alignas(4) std::vector<uint8_t> bytes;

    EmptyDialogTemplate(std::wstring_view title, short widthDlu = 360, short heightDlu = 240,
                        DWORD extraStyle = 0) {
        DLGTEMPLATE hdr{};
        hdr.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER | extraStyle;
        hdr.dwExtendedStyle = 0;
        hdr.cdit = 0;
        hdr.x = 0;
        hdr.y = 0;
        hdr.cx = widthDlu;
        hdr.cy = heightDlu;

        bytes.resize(sizeof(DLGTEMPLATE));
        std::memcpy(bytes.data(), &hdr, sizeof(DLGTEMPLATE));

        auto appendWord = [&]( uint16_t w ) {
            const uint8_t lo = static_cast<uint8_t>(w & 0xFFu);
            const uint8_t hi = static_cast<uint8_t>((w >> 8) & 0xFFu);
            bytes.push_back(lo);
            bytes.push_back(hi);
        };

        appendWord(0); // no menu
        appendWord(0); // default dialog class
        for (wchar_t ch : title) appendWord(static_cast<uint16_t>(ch));
        appendWord(0); // title null terminator
        while (bytes.size() % 4 != 0) bytes.push_back(0);
    }

    const DLGTEMPLATE* Get() const { return reinterpret_cast<const DLGTEMPLATE*>(bytes.data()); }
};

} // namespace DNItemMaker::UI
