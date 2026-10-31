#pragma once

/**
 * @file FlatControls.h
 * @brief Drop-in flat replacements for CEdit, CButton and CTabCtrl that follow the shared Theme.
 *
 * Each class keeps the stock control's behaviour (notifications, keyboard handling, accessibility)
 * and only replaces how it is painted.
 */

#include "UI/MfcCommon.h"

namespace DNItemMaker::UI {

/**
 * @class CFlatEdit
 * @brief Edit control with a single-pixel border that turns accent-colored while focused.
 *
 * Always draws its border: create it with WS_BORDER, and use a plain CEdit for borderless edits.
 * Edits with scroll bars keep their visual-styles theme so the scroll bars stay modern.
 */
class CFlatEdit : public CEdit {
protected:
    void PreSubclassWindow() override;

    afx_msg void OnNcPaint();
    afx_msg void OnPaint();
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnEnable(BOOL bEnable);

    DECLARE_MESSAGE_MAP()

private:
    void DrawFlatBorder();
};

/**
 * @class CFlatButton
 * @brief Rectangular flat push button painted through NM_CUSTOMDRAW (ComCtl32 v6).
 *
 * Primary buttons (SetPrimary, or BS_DEFPUSHBUTTON) carry an accent border instead of bold text.
 * Check boxes, radio buttons and group boxes fall through to stock painting.
 */
class CFlatButton : public CButton {
public:
    /** @brief Marks the button as a primary action (accent border). */
    void SetPrimary(bool primary);

protected:
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    bool m_primary = false;
};

/**
 * @class CFlatGroupBox
 * @brief Group box control that erases its background and draws a clean flat border and header text.
 *
 * Solves the Win32 BS_GROUPBOX interior transparency issue under WS_CLIPCHILDREN.
 */
class CFlatGroupBox : public CButton {
public:
    CFlatGroupBox() = default;

    BOOL Create(LPCTSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID = 0xffff);

protected:
    void PreSubclassWindow() override;

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg LRESULT OnNcHitTest(CPoint point);

    DECLARE_MESSAGE_MAP()
};

/**
 * @class CFlatTabCtrl
 * @brief Tab control with flat tabs: the pane and selected tab share the chrome color,
 *        and a thin accent bar marks the active tab.
 */
class CFlatTabCtrl : public CTabCtrl {
protected:
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();

    DECLARE_MESSAGE_MAP()
};

/**
 * @brief Applies flat styling to stock child controls that need no subclass
 *        (drop-down-list combo boxes get the white "CFD" field theme).
 * @param parent Window whose direct children are restyled.
 */
void ApplyFlatChildStyles(CWnd* parent);

} // namespace DNItemMaker::UI
