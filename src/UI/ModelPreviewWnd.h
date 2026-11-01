#pragma once

/**
 * @file ModelPreviewWnd.h
 * @brief Interactive software-rendered 3D viewport, ACT animation viewer, and telemetry window.
 *
 * Implements an interactive CWnd rendering 3D meshes using the CPU MeshRenderer.
 * Supports Blender-style viewport interaction:
 * - Left-click drag: Camera orbit (yaw and pitch).
 * - Mouse wheel: Smooth geometric zoom.
 * - Right-click: Context menu with render mode and auto-spin toggles.
 * - Blender-style 3D ground plane tiles and interactive XYZ orientation gizmo.
 * - Interactive ACT animation player: forward kinematics skeletal animation,
 *   action selector, play/pause/reset, loop, playback speed, and timeline scrubber.
 */

#include "UI/MfcCommon.h"
#include "Engine/WorkspaceEngine.h"
#include "Graphics/AniParser.h"
#include "Graphics/ActParser.h"

namespace DNItemMaker::UI {

/// Command ID posted when double-clicking the icon preview card.
constexpr UINT kCmdPickIconFromPreview = 49001;

/**
 * @class CModelPreviewWnd
 * @brief Interactive 3D model viewport control with integrated telemetry and ACT animation player.
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

    /** @brief Sets workspace engine reference for loading character animations and models. */
    void SetWorkspaceEngine(Engine::WorkspaceEngine* engine) noexcept { m_engine = engine; }

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

    // --- ACT Animation Player Controls ---
    /** @brief Selects character class for previewing item on character body. */
    void SetCharacterClass(Engine::CharacterClassId classId);

    /** @brief Selects animation action name (Stand, Move_Front, Attack1, etc.). */
    void SetAction(std::string_view actionName);

    /** @brief Starts or pauses animation playback. */
    void SetPlaying(bool play);

    /** @brief Sets loop playback mode. */
    void SetLoop(bool loop) noexcept { m_isLoop = loop; }

    /** @brief Sets animation speed multiplier (0.5x, 1.0x, 1.5x, 2.0x, etc.). */
    void SetSpeed(float speed) noexcept { m_speed = speed; }

    /** @brief Scrubs current playback frame directly. */
    void SetAnimFrame(float frame);

    /** @brief Resets animation playback to start frame. */
    void ResetAnimPlayback();

    /** @brief Returns currently selected character class ID. */
    Engine::CharacterClassId GetCharacterClass() const noexcept { return m_currentClass; }

    /** @brief Returns active action name. */
    std::string_view GetCurrentAction() const noexcept { return m_currentActionName; }

    /** @brief Returns true if animation is currently playing. */
    bool IsPlaying() const noexcept { return m_isPlaying; }

    /** @brief Returns true if animation loop mode is enabled. */
    bool IsLoop() const noexcept { return m_isLoop; }

    /** @brief Returns current playback speed multiplier. */
    float GetSpeed() const noexcept { return m_speed; }

    /** @brief Returns current playback frame. */
    float GetCurrentFrame() const noexcept { return m_animFrame; }

    /** @brief Returns max frame in active action. */
    float GetMaxFrame() const noexcept { return m_maxFrame; }

    /** @brief Returns list of available action names for current character class. */
    std::vector<std::string> GetAvailableActions() const;

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
    CRect GetScrubberRect(const CRect& client) const;
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

    // ACT Animation Viewer state
    Engine::WorkspaceEngine* m_engine = nullptr;
    Engine::CharacterClassId m_currentClass = Engine::CharacterClassId::ItemOnly;
    std::shared_ptr<Engine::CharacterClassBundle> m_charBundle;
    std::vector<Graphics::Matrix4> m_boneWorldMatrices;

    std::string m_currentActionName = "Stand";
    int32_t m_animClipIndex = -1;
    float m_animFrame = 0.0f;
    float m_minFrame = 0.0f;
    float m_maxFrame = 50.0f;
    float m_speed = 1.0f;
    bool m_isPlaying = true;
    bool m_isLoop = true;
    bool m_scrubbing = false;

    CFont m_fontTitle;
    CFont m_fontSmall;
};

} // namespace DNItemMaker::UI
