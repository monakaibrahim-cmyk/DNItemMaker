#pragma once

/**
 * @file ModelPreviewWnd.h
 * @brief Interactive software-rendered 3D viewport and telemetry window.
 *
 * Implements an interactive CWnd rendering 3D meshes using the CPU MeshRenderer.
 * Supports Blender-style viewport interaction:
 * - Left-click drag: Camera orbit (yaw and pitch).
 * - Mouse wheel: Smooth geometric zoom.
 * - Right-click: Context menu with render mode and auto-spin toggles.
 * - Blender-style 3D ground plane tiles and interactive XYZ orientation gizmo.
 */

#include "UI/MfcCommon.h"
#include "Engine/WorkspaceEngine.h"

namespace DNItemMaker::UI {

/// Command ID posted when double-clicking the icon preview card.
constexpr UINT kCmdPickIconFromPreview = 49001;

/**
 * @class CModelPreviewWnd
 * @brief Interactive 3D model viewport control with integrated telemetry overlay.
 */
class CModelPreviewWnd : public CWnd {
public:
    CModelPreviewWnd() = default;
    ~CModelPreviewWnd() override = default;

    /**
     * @brief Creates the viewport child window.
     * @param parent Pointer to parent window.
     * @param rect Bounding coordinates.
     * @param id Child window control ID.
     * @return true on success.
     */
    bool Create(CWnd* parent, const CRect& rect, UINT id);

    /**
     * @brief Updates active preview data with item attributes and 3D model bundle.
     * @param item Pointer to item data.
     * @param iconImage Pointer to 50x50 icon image.
     * @param modelBundle Populated 3D mesh and skin asset bundle.
     */
    void SetPreviewData(const Data::DNItem* item,
                        const Graphics::ImageBuffer* iconImage,
                        Engine::ModelAssetBundle modelBundle);

    /**
     * @brief Updates only the 2D icon preview without rebuilding 3D geometry.
     * @param iconIndex Icon atlas index.
     * @param itemName Display name.
     * @param rank Item rank grade.
     * @param iconImage Decoded icon image buffer.
     */
    void UpdateIconOnly(int32_t iconIndex, const std::string& itemName, int32_t rank,
                        const Graphics::ImageBuffer* iconImage);

    /** @brief Clears model and icon preview state. */
    void ClearPreview();

    /** @brief Resets camera rotation angles and zoom factor to defaults. */
    void ResetCamera();

    /** @brief Enables or disables continuous turntable auto-spin animation. */
    void SetAutoSpin(bool enabled);

    /** @brief Returns true if auto-spin turntable mode is currently active. */
    bool IsAutoSpin() const noexcept { return m_autoSpin; }

    /** @brief Sets active shading mode (TexturedLit, ClayShaded, Wireframe). */
    void SetRenderMode(Graphics::RenderMode mode);

    /** @brief Returns currently active shading mode. */
    Graphics::RenderMode GetRenderMode() const noexcept { return m_renderMode; }

    /** @brief Multiplies current zoom level by a scaling factor. */
    void AdjustZoom(float factor);

    /** @brief Returns current camera zoom factor. */
    float GetZoom() const noexcept { return m_zoom; }

    /** @brief Returns true if a valid 3D model is loaded and ready to render. */
    bool HasLoadedModel() const noexcept { return m_model.hasModel; }

protected:
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();

    DECLARE_MESSAGE_MAP()

private:
    CRect GetIconCardRect(const CRect& client) const;
    CRect GetViewportRect(const CRect& client) const;
    CRect GetTelemetryRect(const CRect& client) const;

    bool m_hasItem = false;
    int32_t m_itemId = 0;
    int32_t m_itemRank = 0;
    int32_t m_iconIndex = -1;
    std::string m_itemName;

    bool m_hasIcon = false;
    Graphics::ImageBuffer m_icon;

    Engine::ModelAssetBundle m_model;
    Graphics::ImageBuffer m_cachedFrame;
    Graphics::RenderScratch m_renderScratch;
    bool m_frameDirty = true;

    float m_yaw = 35.0f;
    float m_pitch = 18.0f;
    float m_zoom = 1.0f;
    Graphics::RenderMode m_renderMode = Graphics::RenderMode::TexturedLit;

    bool m_dragging = false;
    CPoint m_lastMouse{};
    bool m_autoSpin = false;

    CFont m_fontTitle;
    CFont m_fontSmall;
};

} // namespace DNItemMaker::UI
