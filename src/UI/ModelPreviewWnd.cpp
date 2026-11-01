#include "UI/ModelPreviewWnd.h"
#include "UI/Theme.h"
#include "Graphics/MeshRenderer.h"
#include <Common/Logs/Logs.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace DNItemMaker::UI {

namespace {
constexpr UINT_PTR kSpinTimerId = 1001;
constexpr UINT_PTR kAnimTimerId = 1002;

inline bool EqualIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string ToLower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return res;
}

const char* RenderModeLabel(Graphics::RenderMode mode) noexcept {
    switch (mode) {
        case Graphics::RenderMode::Wireframe:   return "Wireframe";
        case Graphics::RenderMode::ClayShaded:  return "Clay Shaded";
        default:                               return "Textured Lit";
    }
}
} // namespace

BEGIN_MESSAGE_MAP(CModelPreviewWnd, CWnd)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CModelPreviewWnd::Create(CWnd* parent, const CRect& rect, UINT id) {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
                                      ::LoadCursor(nullptr, IDC_ARROW),
                                      nullptr, nullptr);
    if (!CWnd::Create(cls, L"ModelPreview", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                      rect, parent, id)) {
        return false;
    }

    const int dpi = GetWindowDpi(GetSafeHwnd());
    m_fontTitle.CreateFontW(-DpiScale(14, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontSmall.CreateFontW(-DpiScale(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return true;
}

void CModelPreviewWnd::SetPreviewData(const Data::DNItem* item,
                                      const Graphics::ImageBuffer* iconImage,
                                      Engine::ModelAssetBundle modelBundle) {
    if (!item) {
        ClearPreview();
        return;
    }
    m_hasItem = true;
    m_itemId = item->id;
    m_itemRank = item->rank;
    m_iconIndex = item->iconIndex;
    m_itemName = item->name;

    if (iconImage && iconImage->width > 0 && iconImage->height > 0) {
        m_icon = *iconImage;
        m_hasIcon = true;
    } else {
        m_icon = {};
        m_hasIcon = false;
    }

    m_model = std::move(modelBundle);
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::UpdateIconOnly(int32_t iconIndex, const std::string& itemName, int32_t rank,
                                      const Graphics::ImageBuffer* iconImage) {
    m_hasItem = true;
    m_iconIndex = iconIndex;
    m_itemName = itemName;
    m_itemRank = rank;
    if (iconImage && iconImage->width > 0 && iconImage->height > 0) {
        m_icon = *iconImage;
        m_hasIcon = true;
    } else {
        m_icon = {};
        m_hasIcon = false;
    }
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::ClearPreview() {
    m_hasItem = false;
    m_hasIcon = false;
    m_icon = {};
    m_model = {};
    m_cachedFrame = {};
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::ResetCamera() {
    m_yaw = 35.0f;
    m_pitch = 18.0f;
    m_zoom = 1.0f;
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::SetAutoSpin(bool enabled) {
    if (m_autoSpin == enabled) return;
    m_autoSpin = enabled;
    if (GetSafeHwnd()) {
        if (m_autoSpin) {
            SetTimer(kSpinTimerId, 16, nullptr);
        } else {
            KillTimer(kSpinTimerId);
        }
    }
}

void CModelPreviewWnd::SetRenderMode(Graphics::RenderMode mode) {
    if (m_renderMode != mode) {
        m_renderMode = mode;
        m_frameDirty = true;
        if (GetSafeHwnd()) Invalidate(FALSE);
    }
}

void CModelPreviewWnd::AdjustZoom(float factor) {
    m_zoom = std::clamp(m_zoom * factor, 0.35f, 4.0f);
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::SetCharacterClass(Engine::CharacterClassId classId) {
    m_currentClass = classId;
    m_charBundle = nullptr;
    m_animClipIndex = -1;
    m_boneWorldMatrices.clear();
    m_animFrame = 0.0f;
    m_minFrame = 0.0f;
    m_maxFrame = 50.0f;

    if (m_currentClass != Engine::CharacterClassId::ItemOnly && m_engine) {
        m_charBundle = m_engine->GetCharacterClass(m_currentClass);
        if (m_charBundle && m_charBundle->animSet && !m_charBundle->animSet->clips.empty()) {
            SetAction("Stand");
            if (m_isPlaying && GetSafeHwnd()) {
                SetTimer(kAnimTimerId, 33, nullptr);
            }
        }
    } else {
        if (GetSafeHwnd()) KillTimer(kAnimTimerId);
    }
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::SetAction(std::string_view actionName) {
    m_currentActionName = actionName;
    m_animClipIndex = -1;
    m_minFrame = 0.0f;
    m_maxFrame = 50.0f;

    if (!m_charBundle || !m_charBundle->animSet || m_charBundle->animSet->clips.empty()) {
        m_animFrame = 0.0f;
        m_frameDirty = true;
        if (GetSafeHwnd()) Invalidate(FALSE);
        return;
    }

    const auto& animSet = *m_charBundle->animSet;

    // 1. Check actions list from .act file
    if (m_charBundle->actions && m_charBundle->actions->valid) {
        if (const auto* act = m_charBundle->actions->FindAction(std::string(actionName))) {
            m_minFrame = 0.0f;
            m_maxFrame = static_cast<float>(std::max<uint32_t>(1, act->length));
            if (!act->linkAniName.empty()) {
                m_animClipIndex = animSet.FindClipIndex(act->linkAniName);
            }
        }
    }

    // 2. If clip not found, look up clip by action name directly
    if (m_animClipIndex < 0) {
        m_animClipIndex = animSet.FindClipIndex(std::string(actionName));
    }

    // 3. Fallback: search for keyword in clip names (e.g., "Stand", "Run", "Attack")
    if (m_animClipIndex < 0) {
        const std::string lowerAct = ToLower(actionName);
        for (size_t i = 0; i < animSet.clips.size(); ++i) {
            const std::string lowerClip = ToLower(animSet.clips[i].name);
            if (lowerClip.find(lowerAct) != std::string::npos) {
                m_animClipIndex = static_cast<int32_t>(i);
                m_minFrame = 0.0f;
                m_maxFrame = static_cast<float>(animSet.clips[i].length);
                break;
            }
        }
    }

    // 4. Default to first clip if still not found
    if (m_animClipIndex < 0 && !animSet.clips.empty()) {
        m_animClipIndex = 0;
        m_minFrame = 0.0f;
        m_maxFrame = static_cast<float>(animSet.clips[0].length);
    }

    if (m_animClipIndex >= 0 && static_cast<size_t>(m_animClipIndex) < animSet.clips.size()) {
        if (m_maxFrame <= m_minFrame) {
            m_maxFrame = static_cast<float>(animSet.clips[static_cast<size_t>(m_animClipIndex)].length);
        }
    }

    m_animFrame = m_minFrame;
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::SetPlaying(bool play) {
    if (m_isPlaying == play) return;
    m_isPlaying = play;
    if (GetSafeHwnd()) {
        if (m_isPlaying && m_currentClass != Engine::CharacterClassId::ItemOnly) {
            SetTimer(kAnimTimerId, 33, nullptr);
        } else {
            KillTimer(kAnimTimerId);
        }
        Invalidate(FALSE);
    }
}

void CModelPreviewWnd::SetAnimFrame(float frame) {
    m_animFrame = std::clamp(frame, m_minFrame, std::max(m_minFrame + 1.0f, m_maxFrame));
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CModelPreviewWnd::ResetAnimPlayback() {
    m_animFrame = m_minFrame;
    m_frameDirty = true;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

std::vector<std::string> CModelPreviewWnd::GetAvailableActions() const {
    std::vector<std::string> res;
    if (!m_charBundle) return res;

    if (m_charBundle->actions && m_charBundle->actions->valid && !m_charBundle->actions->actions.empty()) {
        for (const auto& a : m_charBundle->actions->actions) {
            if (!a.name.empty()) res.push_back(a.name);
        }
    } else if (m_charBundle->animSet) {
        for (const auto& c : m_charBundle->animSet->clips) {
            if (!c.name.empty()) res.push_back(c.name);
        }
    }
    return res;
}

void CModelPreviewWnd::OnDestroy() {
    if (m_autoSpin) KillTimer(kSpinTimerId);
    KillTimer(kAnimTimerId);
    CWnd::OnDestroy();
}

BOOL CModelPreviewWnd::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE;
}

void CModelPreviewWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    m_frameDirty = true;
    Invalidate(FALSE);
}

CRect CModelPreviewWnd::GetIconCardRect(const CRect& client) const {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int h = DpiScale(92, dpi);
    return CRect(client.left, client.top, client.right, std::min<LONG>(client.bottom, client.top + h));
}

CRect CModelPreviewWnd::GetTelemetryRect(const CRect& client) const {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int h = DpiScale(64, dpi);
    const LONG top = std::max<LONG>(client.top, client.bottom - h);
    return CRect(client.left, top, client.right, client.bottom);
}

CRect CModelPreviewWnd::GetScrubberRect(const CRect& client) const {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int scrubH = DpiScale(26, dpi);
    const CRect teleRc = GetTelemetryRect(client);
    const LONG bottom = teleRc.top - DpiScale(4, dpi);
    const LONG top = bottom - scrubH;
    return CRect(client.left, top, client.right, bottom);
}

CRect CModelPreviewWnd::GetViewportRect(const CRect& client) const {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int gap = DpiScale(4, dpi);
    const CRect iconRc = GetIconCardRect(client);
    const CRect scrubRc = GetScrubberRect(client);
    const LONG top = std::min<LONG>(client.bottom, iconRc.bottom + gap);
    const LONG bottom = std::max<LONG>(top, scrubRc.top - gap);
    return CRect(client.left, top, client.right, bottom);
}

void CModelPreviewWnd::OnPaint() {
    CPaintDC paintDC(this);
    CRect client;
    GetClientRect(&client);
    if (client.Width() <= 0 || client.Height() <= 0) return;

    CDC memDC;
    memDC.CreateCompatibleDC(&paintDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paintDC, client.Width(), client.Height());
    CBitmap* oldBmp = memDC.SelectObject(&bmp);

    const int dpi = GetWindowDpi(GetSafeHwnd());
    memDC.FillSolidRect(&client, Theme::kChrome);
    memDC.SetBkMode(TRANSPARENT);

    const CRect iconCard = GetIconCardRect(client);
    const COLORREF cardBg = RGB(22, 27, 40);
    const COLORREF slotBorder = RGB(62, 74, 102);
    memDC.FillSolidRect(&iconCard, cardBg);
    memDC.Draw3dRect(&iconCard, slotBorder, slotBorder);

    const int pad = DpiScale(10, dpi);
    const int s1 = DpiScale(50, dpi);
    const int s2 = DpiScale(68, dpi);
    const CRect slot1(iconCard.left + pad, iconCard.top + (iconCard.Height() - s1) / 2,
                      iconCard.left + pad + s1, iconCard.top + (iconCard.Height() - s1) / 2 + s1);
    const CRect slot2(slot1.right + pad, iconCard.top + (iconCard.Height() - s2) / 2,
                      slot1.right + pad + s2, iconCard.top + (iconCard.Height() - s2) / 2 + s2);

    const COLORREF slotBg = RGB(14, 17, 25);
    memDC.FillSolidRect(&slot1, slotBg);
    memDC.FillSolidRect(&slot2, slotBg);

    if (m_hasIcon) {
        BlitImageBuffer(memDC.GetSafeHdc(), slot1, m_icon, slotBg, false);
        BlitImageBuffer(memDC.GetSafeHdc(), slot2, m_icon, slotBg, true);
    } else {
        CFont* oldF = memDC.SelectObject(&m_fontSmall);
        memDC.SetTextColor(RGB(120, 128, 145));
        CRect r1 = slot1, r2 = slot2;
        memDC.DrawTextW(L"N/A", &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        memDC.DrawTextW(L"No Icon", &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        memDC.SelectObject(oldF);
    }
    memDC.Draw3dRect(&slot1, RGB(92, 102, 126), RGB(52, 58, 72));
    memDC.Draw3dRect(&slot2, RankTextColor(m_itemRank, true), RGB(52, 58, 72));

    const int textLeft = slot2.right + pad;
    if (textLeft < iconCard.right - pad) {
        CRect titleRc(textLeft, iconCard.top + DpiScale(8, dpi), iconCard.right - pad, iconCard.top + DpiScale(30, dpi));
        CFont* oldF = memDC.SelectObject(&m_fontTitle);
        memDC.SetTextColor(m_hasItem ? RankTextColor(m_itemRank, true) : RGB(180, 185, 195));
        const CString wTitle = m_hasItem ? Utf8ToWide(m_itemName) : CString(L"No Item Selected");
        memDC.DrawTextW(wTitle, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        memDC.SelectObject(&m_fontSmall);
        memDC.SetTextColor(RGB(178, 188, 208));
        CRect metaRc(textLeft, titleRc.bottom + DpiScale(2, dpi), iconCard.right - pad, iconCard.bottom - DpiScale(6, dpi));
        if (m_hasItem && m_iconIndex >= 0) {
            const int page = m_iconIndex / 200 + 1;
            const int tile = m_iconIndex % 200;
            const std::string iconInfo = std::format(
                "2D Icon #{}   |   Atlas: itemicon{:02d}.dds (Col {}, Row {})\r\n"
                "Click this card or 'Open Icon Atlas...' below to visually pick a new icon.",
                m_iconIndex, page, tile % 10, tile / 10);
            memDC.DrawTextW(Utf8ToWide(iconInfo), &metaRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        } else {
            memDC.DrawTextW(L"Select an item from the browser to inspect its 2D DDS icon and 3D Eternity mesh.",
                            &metaRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
        }
        memDC.SelectObject(oldF);
    }

    // 3D Viewport
    const CRect vp = GetViewportRect(client);
    if (vp.Width() > 8 && vp.Height() > 8) {
        const uint32_t rw = static_cast<uint32_t>(std::clamp(vp.Width() - 2, 32, 1920));
        const uint32_t rh = static_cast<uint32_t>(std::clamp(vp.Height() - 2, 32, 1080));

        if (m_frameDirty || m_cachedFrame.width != rw || m_cachedFrame.height != rh) {
            if (m_currentClass == Engine::CharacterClassId::ItemOnly || !m_charBundle || !m_charBundle->isLoaded) {
                // Item only preview
                const bool hasMesh = m_model.hasModel && m_model.mesh && m_model.mesh->valid;
                static const Graphics::ParsedMesh s_emptyMesh{};
                const auto& meshToRender = hasMesh ? *m_model.mesh : s_emptyMesh;
                Graphics::MeshRenderer::RenderInto(meshToRender, m_model.materials, m_yaw, m_pitch,
                                                   rw, rh, m_cachedFrame, m_renderScratch,
                                                   m_zoom, m_renderMode);
            } else {
                // Character class preview + equipped item
                if (m_charBundle->animSet) {
                    m_charBundle->animSet->EvaluatePose(m_animClipIndex, m_animFrame, m_boneWorldMatrices);
                }

                std::vector<Graphics::RenderObject> renderObjs;

                // Character body object
                if (m_charBundle->mesh && m_charBundle->mesh->valid) {
                    Graphics::RenderObject bodyObj{};
                    bodyObj.mesh = m_charBundle->mesh.get();
                    bodyObj.materials = m_charBundle->materials;
                    bodyObj.boneWorldMatrices = &m_boneWorldMatrices;
                    bodyObj.skeleton = m_charBundle->animSet.get();
                    bodyObj.transform = Graphics::Matrix4::Identity();
                    renderObjs.push_back(bodyObj);
                }

                // Equipped Item object (if valid)
                if (m_model.hasModel && m_model.mesh && m_model.mesh->valid) {
                    Graphics::RenderObject itemObj{};
                    itemObj.mesh = m_model.mesh.get();
                    itemObj.materials = m_model.materials;

                    const bool isWeapon = (m_hasItem && (m_model.meshPath.string().find("weapon") != std::string::npos ||
                                                         m_model.skinPath.string().find("weapon") != std::string::npos));

                    if (isWeapon) {
                        int32_t handBone = -1;
                        if (m_charBundle->animSet) {
                            handBone = m_charBundle->animSet->FindBoneIndex("~BoxBone01");
                            if (handBone < 0) handBone = m_charBundle->animSet->FindBoneIndex("Bip01 R Hand");
                            if (handBone < 0) handBone = m_charBundle->animSet->FindBoneIndex("R_Hand");
                        }
                        if (handBone >= 0 && static_cast<size_t>(handBone) < m_boneWorldMatrices.size()) {
                            itemObj.transform = m_boneWorldMatrices[static_cast<size_t>(handBone)];
                        } else {
                            itemObj.transform = Graphics::Matrix4::Identity();
                        }
                        itemObj.boneWorldMatrices = nullptr;
                    } else {
                        // Armor / Costume skins share character skeleton
                        itemObj.boneWorldMatrices = &m_boneWorldMatrices;
                        itemObj.skeleton = m_charBundle->animSet.get();
                        itemObj.transform = Graphics::Matrix4::Identity();
                    }
                    renderObjs.push_back(itemObj);
                }

                Graphics::MeshRenderer::RenderSceneInto(renderObjs, m_yaw, m_pitch,
                                                       rw, rh, m_cachedFrame, m_renderScratch,
                                                       m_zoom, m_renderMode);
            }
            m_frameDirty = false;
        }

        CRect innerVp = vp;
        innerVp.DeflateRect(1, 1);
        BlitImageBuffer(memDC.GetSafeHdc(), innerVp, m_cachedFrame, RGB(22, 26, 38), true);

        CFont* oldF = memDC.SelectObject(&m_fontSmall);
        memDC.SetTextColor(RGB(190, 204, 232));
        CRect hintRc(innerVp.left + DpiScale(10, dpi), innerVp.top + DpiScale(8, dpi),
                     innerVp.right - DpiScale(88, dpi), innerVp.top + DpiScale(26, dpi));
        const std::string angleText = std::format(
            "[{}]  Yaw {:.0f}°  Pitch {:.0f}°  Zoom {:.0f}%   (LMB: Orbit  |  Wheel: Zoom  |  RMB: Reset)",
            RenderModeLabel(m_renderMode), m_yaw, m_pitch, m_zoom * 100.0f);
        memDC.DrawTextW(Utf8ToWide(angleText), &hintRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

        const bool hasActiveMesh = (m_currentClass != Engine::CharacterClassId::ItemOnly && m_charBundle && m_charBundle->isLoaded) ||
                                   (m_model.hasModel && m_model.mesh && m_model.mesh->valid);
        if (!hasActiveMesh) {
            memDC.SetTextColor(RGB(152, 164, 188));
            CRect msgRc = innerVp;
            msgRc.top += DpiScale(30, dpi);
            const CString placeholder = m_hasItem
                                            ? Utf8ToWide(m_model.statusSummary.empty()
                                                             ? "No 3D equipment mesh bound to this item."
                                                             : m_model.statusSummary)
                                            : CString(L"3D Equipment Viewport (.skn / .msh / .dds)");
            memDC.DrawTextW(placeholder, &msgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        memDC.SelectObject(oldF);
        memDC.Draw3dRect(&vp, slotBorder, slotBorder);
    }

    // Scrubber Bar
    const CRect scrub = GetScrubberRect(client);
    memDC.FillSolidRect(&scrub, RGB(22, 27, 40));
    memDC.Draw3dRect(&scrub, slotBorder, slotBorder);

    CFont* oldF = memDC.SelectObject(&m_fontSmall);
    const int scrubPad = DpiScale(10, dpi);
    const int leftLabelW = DpiScale(180, dpi);
    const int rightLabelW = DpiScale(170, dpi);

    // Left label: Status + Action
    CRect leftLabelRc(scrub.left + scrubPad, scrub.top, scrub.left + leftLabelW, scrub.bottom);
    memDC.SetTextColor(m_isPlaying ? RGB(78, 201, 176) : RGB(220, 160, 60));
    std::string actTitle = std::format("{} Action: {}", m_isPlaying ? "▶" : "⏸", m_currentActionName);
    memDC.DrawTextW(Utf8ToWide(actTitle), &leftLabelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Right label: Frame count
    CRect rightLabelRc(scrub.right - rightLabelW - scrubPad, scrub.top, scrub.right - scrubPad, scrub.bottom);
    memDC.SetTextColor(RGB(180, 195, 220));
    std::string frameText = std::format("Frame: {:.0f} / {:.0f} (30 FPS)", m_animFrame, m_maxFrame);
    memDC.DrawTextW(Utf8ToWide(frameText), &rightLabelRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Center Track
    const int trackLeft = scrub.left + leftLabelW + scrubPad;
    const int trackRight = scrub.right - rightLabelW - scrubPad * 2;
    if (trackRight > trackLeft + 20) {
        const int trackH = DpiScale(8, dpi);
        const int trackY = scrub.top + (scrub.Height() - trackH) / 2;
        CRect trackRc(trackLeft, trackY, trackRight, trackY + trackH);
        memDC.FillSolidRect(&trackRc, RGB(14, 17, 25));
        memDC.Draw3dRect(&trackRc, RGB(50, 60, 80), RGB(50, 60, 80));

        // Progress fill
        const float span = std::max(1.0f, m_maxFrame - m_minFrame);
        const float frac = std::clamp((m_animFrame - m_minFrame) / span, 0.0f, 1.0f);
        const int fillW = static_cast<int>(frac * static_cast<float>(trackRc.Width()));
        if (fillW > 0) {
            CRect fillRc(trackRc.left, trackRc.top, trackRc.left + fillW, trackRc.bottom);
            memDC.FillSolidRect(&fillRc, RGB(0, 122, 204));
        }

        // Draggable Thumb
        const int thumbW = DpiScale(10, dpi);
        const int thumbH = DpiScale(16, dpi);
        const int thumbX = trackRc.left + fillW - thumbW / 2;
        const int thumbY = scrub.top + (scrub.Height() - thumbH) / 2;
        CRect thumbRc(thumbX, thumbY, thumbX + thumbW, thumbY + thumbH);
        memDC.FillSolidRect(&thumbRc, RGB(210, 225, 250));
        memDC.Draw3dRect(&thumbRc, RGB(255, 255, 255), RGB(100, 120, 150));
    }
    memDC.SelectObject(oldF);

    // Telemetry
    const CRect tele = GetTelemetryRect(client);
    memDC.FillSolidRect(&tele, RGB(22, 27, 40));
    memDC.Draw3dRect(&tele, slotBorder, slotBorder);

    oldF = memDC.SelectObject(&m_fontSmall);
    memDC.SetTextColor(RGB(194, 204, 224));
    CRect teleText = tele;
    teleText.DeflateRect(pad, DpiScale(6, dpi));

    std::string teleLines;
    if (m_currentClass != Engine::CharacterClassId::ItemOnly && m_charBundle && m_charBundle->isLoaded) {
        const auto& cMesh = *m_charBundle->mesh;
        std::string equipInfo;
        if (m_model.hasModel && m_model.mesh && m_model.mesh->valid) {
            equipInfo = std::format("   |   Equipped Item: #{} '{}' ({} tris)",
                                    m_itemId, m_itemName, m_model.mesh->GetTotalTriangles());
        } else {
            equipInfo = "   |   Equipped Item: (No 3D mesh)";
        }
        const size_t clipCount = m_charBundle->animSet ? m_charBundle->animSet->clips.size() : size_t{0};
        const int32_t aniVer = m_charBundle->animSet ? m_charBundle->animSet->version : 0;
        teleLines = std::format(
            "Character: {}   |   Mesh v{} ({} verts, {} tris, {} bones)   |   Action: '{}' [Clip #{}, Frame {:.0f}/{:.0f}]\r\nAnimation Set: {} clips (Eternity .ani v{}){}",
            m_charBundle->className,
            cMesh.version,
            cMesh.GetTotalVertices(),
            cMesh.GetTotalTriangles(),
            m_boneWorldMatrices.size(),
            m_currentActionName,
            m_animClipIndex,
            m_animFrame,
            m_maxFrame,
            clipCount,
            aniVer,
            equipInfo);
    } else if (m_model.hasModel && m_model.mesh && m_model.mesh->valid) {
        const auto& mesh = *m_model.mesh;
        const int32_t skinVer = m_model.skin ? m_model.skin->version : 0;
        const auto ext = mesh.bbox.GetExtents();
        std::string texList;
        for (size_t i = 0; i < m_model.textureNames.size(); ++i) {
            if (i > 0) texList += ", ";
            texList += m_model.textureNames[i];
        }
        if (texList.empty()) texList = "(untextured)";
        teleLines = std::format(
            "Skin: {} (v{})   |   Mesh: {} (v{})   |   Submeshes: {}   |   Vertices: {}   |   Triangles: {}   |   Bones: {}\r\nBounding Box: {:.1f} x {:.1f} x {:.1f}   |   Bound DDS Textures: {}",
            m_model.skinPath.filename().string(), skinVer,
            m_model.meshPath.filename().string(), mesh.version,
            mesh.submeshes.size(), mesh.GetTotalVertices(),
            mesh.GetTotalTriangles(), mesh.bones.size(),
            ext.x, ext.y, ext.z, texList);
    } else if (m_hasItem) {
        teleLines = std::format("Item #{}   |   {}", m_itemId,
                                m_model.statusSummary.empty()
                                    ? "No 3D skin/mesh asset associated (use Visual Cloner below to bind a weapon/armor skin)."
                                    : m_model.statusSummary);
    } else {
        teleLines = "Eternity Engine 3D Asset & Animation Inspector\r\nSupports .skn v10, .msh v13, .ani v11, .act v6, and DXT1/DXT3/DXT5/RGBA .dds textures.";
    }
    memDC.DrawTextW(Utf8ToWide(teleLines), &teleText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    memDC.SelectObject(oldF);

    paintDC.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(oldBmp);
}

void CModelPreviewWnd::OnLButtonDown(UINT nFlags, CPoint point) {
    SetFocus();
    CRect client;
    GetClientRect(&client);

    // 1. Icon card click
    if (GetIconCardRect(client).PtInRect(point)) {
        if (CWnd* parent = GetParent()) {
            parent->PostMessageW(WM_COMMAND, MAKEWPARAM(kCmdPickIconFromPreview, BN_CLICKED),
                                 reinterpret_cast<LPARAM>(GetSafeHwnd()));
        }
        return;
    }

    // 2. Scrubber click
    const CRect scrubRc = GetScrubberRect(client);
    if (scrubRc.PtInRect(point)) {
        const int dpi = GetWindowDpi(GetSafeHwnd());
        const int leftLabelW = DpiScale(180, dpi);
        const int rightLabelW = DpiScale(170, dpi);
        const int scrubPad = DpiScale(10, dpi);
        const int trackLeft = scrubRc.left + leftLabelW + scrubPad;
        const int trackRight = scrubRc.right - rightLabelW - scrubPad * 2;
        if (point.x >= trackLeft && point.x <= trackRight && trackRight > trackLeft) {
            m_scrubbing = true;
            SetCapture();
            const float frac = static_cast<float>(point.x - trackLeft) / static_cast<float>(trackRight - trackLeft);
            const float span = std::max(1.0f, m_maxFrame - m_minFrame);
            m_animFrame = m_minFrame + frac * span;
            m_frameDirty = true;
            Invalidate(FALSE);
            return;
        }
    }

    // 3. Viewport drag (Camera Orbit)
    if (GetViewportRect(client).PtInRect(point)) {
        m_dragging = true;
        m_lastMouse = point;
        SetCapture();
    }

    CWnd::OnLButtonDown(nFlags, point);
}

void CModelPreviewWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_scrubbing) {
        m_scrubbing = false;
        ReleaseCapture();
    }
    if (m_dragging) {
        m_dragging = false;
        ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}

void CModelPreviewWnd::OnRButtonUp(UINT nFlags, CPoint point) {
    CRect client;
    GetClientRect(&client);
    if (GetViewportRect(client).PtInRect(point)) {
        ResetCamera();
        return;
    }
    CWnd::OnRButtonUp(nFlags, point);
}

void CModelPreviewWnd::OnMouseMove(UINT nFlags, CPoint point) {
    if (m_scrubbing) {
        const int dpi = GetWindowDpi(GetSafeHwnd());
        CRect client;
        GetClientRect(&client);
        const CRect scrubRc = GetScrubberRect(client);
        const int leftLabelW = DpiScale(180, dpi);
        const int rightLabelW = DpiScale(170, dpi);
        const int scrubPad = DpiScale(10, dpi);
        const int trackLeft = scrubRc.left + leftLabelW + scrubPad;
        const int trackRight = scrubRc.right - rightLabelW - scrubPad * 2;
        if (trackRight > trackLeft) {
            const float frac = std::clamp(static_cast<float>(point.x - trackLeft) / static_cast<float>(trackRight - trackLeft), 0.0f, 1.0f);
            const float span = std::max(1.0f, m_maxFrame - m_minFrame);
            m_animFrame = m_minFrame + frac * span;
            m_frameDirty = true;
            Invalidate(FALSE);
        }
        return;
    }

    if (m_dragging && (nFlags & MK_LBUTTON)) {
        const int dx = point.x - m_lastMouse.x;
        const int dy = point.y - m_lastMouse.y;
        m_lastMouse = point;

        m_yaw = std::fmod(m_yaw + static_cast<float>(dx) * 0.7f, 360.0f);
        if (m_yaw < 0.0f) m_yaw += 360.0f;
        m_pitch = std::clamp(m_pitch - static_cast<float>(dy) * 0.5f, -85.0f, 85.0f);

        m_frameDirty = true;
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

BOOL CModelPreviewWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    const float factor = (zDelta > 0) ? 1.15f : 0.87f;
    AdjustZoom(factor);
    return TRUE;
}

void CModelPreviewWnd::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kSpinTimerId && m_autoSpin) {
        m_yaw = std::fmod(m_yaw + 1.2f, 360.0f);
        m_frameDirty = true;
        Invalidate(FALSE);
    } else if (nIDEvent == kAnimTimerId) {
        if (m_isPlaying && m_currentClass != Engine::CharacterClassId::ItemOnly && m_charBundle && m_charBundle->animSet) {
            m_animFrame += 1.0f * m_speed;
            if (m_animFrame > m_maxFrame) {
                if (m_isLoop) {
                    const float span = std::max(1.0f, m_maxFrame - m_minFrame);
                    m_animFrame = m_minFrame + std::fmod(m_animFrame - m_minFrame, span);
                } else {
                    m_animFrame = m_maxFrame;
                    m_isPlaying = false;
                }
            }
            m_frameDirty = true;
            Invalidate(FALSE);
        }
    } else {
        CWnd::OnTimer(nIDEvent);
    }
}

} // namespace DNItemMaker::UI
