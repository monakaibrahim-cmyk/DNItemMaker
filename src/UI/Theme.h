#pragma once

/**
 * @file Theme.h
 * @brief Shared visual language for the classic-layout / modern-finish MFC interface.
 *
 * Neutral chrome, white input wells, thin single-pixel borders, and one muted accent.
 * Every window that paints a background or answers WM_CTLCOLOR* goes through here so
 * the surfaces stay seamless.
 */

#include "UI/MfcCommon.h"

namespace DNItemMaker::UI::Theme {

inline constexpr COLORREF kChrome     = RGB(245, 245, 245); ///< Window, dialog and pane background.
inline constexpr COLORREF kSurface    = RGB(255, 255, 255); ///< Edits, lists, combo fields.
inline constexpr COLORREF kBorder     = RGB(200, 200, 200); ///< Single-pixel frames.
inline constexpr COLORREF kBorderHot  = RGB(150, 150, 150); ///< Hovered frames.
inline constexpr COLORREF kAccent     = RGB(70, 130, 180);  ///< Focus, primary action, active selection.
inline constexpr COLORREF kAccentSoft = RGB(221, 232, 242); ///< Pressed state, inactive selection.
inline constexpr COLORREF kText       = RGB(28, 28, 28);
inline constexpr COLORREF kTextMuted  = RGB(115, 115, 115);

/// Process-lifetime solid brushes for the palette surfaces.
HBRUSH ChromeBrush();
HBRUSH SurfaceBrush();

/**
 * @struct FontSet
 * @brief Segoe UI type ramp for one top-level window at one DPI.
 *
 * Build a fresh set when the DPI changes, apply it to the children, then release the old one.
 */
struct FontSet {
    CFont regular;   ///< Segoe UI 9pt.
    CFont bold;      ///< Segoe UI Semibold 9pt.
    CFont secondary; ///< Segoe UI 8pt, for metadata lines.
    CFont title;     ///< Segoe UI Semibold 11pt, for dialog headings.
    CFont mono;      ///< Consolas 9pt, for raw values and reports.

    /** @brief Creates the ramp at the given DPI. */
    void Build(int dpi);
};

/**
 * @brief Dialog base units for a font: the same values MapDialogRect uses for a DS_SETFONT dialog.
 * @param wnd Window whose DC measures the font.
 * @param font Font to measure.
 */
CSize DialogBaseUnits(CWnd* wnd, CFont& font);
inline int DluX(int dlu, CSize baseUnits) noexcept { return ::MulDiv(dlu, baseUnits.cx, 4); }
inline int DluY(int dlu, CSize baseUnits) noexcept { return ::MulDiv(dlu, baseUnits.cy, 8); }

/**
 * @brief Shared WM_CTLCOLOR* policy.
 * @return Brush to return from OnCtlColor, or nullptr to fall through to the base class.
 */
HBRUSH CtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

/** @brief Draws a single-pixel rectangle outline. */
inline void FrameRect1px(CDC& dc, const CRect& rc, COLORREF color) {
    dc.FillSolidRect(rc.left, rc.top, rc.Width(), 1, color);
    dc.FillSolidRect(rc.left, rc.bottom - 1, rc.Width(), 1, color);
    dc.FillSolidRect(rc.left, rc.top, 1, rc.Height(), color);
    dc.FillSolidRect(rc.right - 1, rc.top, 1, rc.Height(), color);
}

} // namespace DNItemMaker::UI::Theme
