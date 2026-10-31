#include "UI/ModelPreviewWnd.h"
#include "UI/Theme.h"

#include "Graphics/MeshRenderer.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace DNItemMaker::UI {

namespace {
constexpr UINT_PTR kSpinTimerId = 1001;

const char* RenderModeLabel(Graphics::RenderMode mode) noexcept {
    switch (mode) {
        case Graphics::RenderMode::ClayShaded: return "Clay Shaded";
        case Graphics::RenderMode::Wireframe:  return "Wireframe";
        case Graphics::RenderMode::TexturedLit:
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
    if (!CWnd::Create(cls, L"ModelPreview", WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect, parent, id)) {
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

void CModelPreviewWnd::OnDestroy() {
    if (m_autoSpin) {
        KillTimer(kSpinTimerId);
    }
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
    const int h = DpiScale(72, dpi);
    const LONG top = std::max<LONG>(client.top, client.bottom - h);
    return CRect(client.left, top, client.right, client.bottom);
}

CRect CModelPreviewWnd::GetViewportRect(const CRect& client) const {
    const int dpi = GetWindowDpi(GetSafeHwnd());
    const int gap = DpiScale(6, dpi);
    const CRect iconRc = GetIconCardRect(client);
    const CRect teleRc = GetTelemetryRect(client);
    const LONG top = std::min<LONG>(client.bottom, iconRc.bottom + gap);
    const LONG bottom = std::max<LONG>(top, teleRc.top - gap);
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

    const CRect vp = GetViewportRect(client);
    if (vp.Width() > 8 && vp.Height() > 8) {
        const uint32_t rw = static_cast<uint32_t>(std::clamp(vp.Width() - 2, 32, 1920));
        const uint32_t rh = static_cast<uint32_t>(std::clamp(vp.Height() - 2, 32, 1080));
        const bool hasMesh = m_model.hasModel && m_model.mesh && m_model.mesh->valid;
        static const Graphics::ParsedMesh s_emptyMesh{};
        const auto& meshToRender = hasMesh ? *m_model.mesh : s_emptyMesh;

        if (m_frameDirty || m_cachedFrame.width != rw || m_cachedFrame.height != rh) {
            Graphics::MeshRenderer::RenderInto(meshToRender, m_model.materials, m_yaw, m_pitch,
                                               rw, rh, m_cachedFrame, m_renderScratch,
                                               m_zoom, m_renderMode);
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

        if (!hasMesh) {
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

    const CRect tele = GetTelemetryRect(client);
    memDC.FillSolidRect(&tele, RGB(22, 27, 40));
    memDC.Draw3dRect(&tele, slotBorder, slotBorder);

    CFont* oldF = memDC.SelectObject(&m_fontSmall);
    memDC.SetTextColor(RGB(194, 204, 224));
    CRect teleText = tele;
    teleText.DeflateRect(pad, DpiScale(6, dpi));

    std::string teleLines;
    if (m_model.hasModel && m_model.mesh && m_model.mesh->valid) {
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
            "Skin: {} (v{})   |   Mesh: {} (v{})   |   Submeshes: {}   |   Vertices: {}   |   Triangles: {}   |   Bones: {}\r\n"
            "Bounding Box: {:.1f} x {:.1f} x {:.1f}   |   Bound DDS Textures: {}",
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
        teleLines = "Eternity Engine 3D Asset Inspector\r\nSupports .skn v10, .msh v13, and DXT1/DXT3/DXT5/RGBA .dds textures.";
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
    if (GetIconCardRect(client).PtInRect(point)) {
        if (CWnd* parent = GetParent()) {
            parent->PostMessageW(WM_COMMAND, MAKEWPARAM(kCmdPickIconFromPreview, BN_CLICKED),
                                 reinterpret_cast<LPARAM>(GetSafeHwnd()));
        }
        return;
    }
    if (GetViewportRect(client).PtInRect(point) && m_model.hasModel) {
        m_dragging = true;
        m_lastMouse = point;
        SetCapture();
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CModelPreviewWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_dragging) {
        m_dragging = false;
        ::ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}

void CModelPreviewWnd::OnRButtonUp(UINT nFlags, CPoint point) {
    CRect client;
    GetClientRect(&client);
    if (GetViewportRect(client).PtInRect(point)) {
        ResetCamera();
    }
    CWnd::OnRButtonUp(nFlags, point);
}

void CModelPreviewWnd::OnMouseMove(UINT nFlags, CPoint point) {
    if (m_dragging) {
        const int dx = point.x - m_lastMouse.x;
        const int dy = point.y - m_lastMouse.y;
        m_lastMouse = point;
        m_yaw = std::fmod(m_yaw + static_cast<float>(dx) * 0.65f, 360.0f);
        if (m_yaw < 0.0f) m_yaw += 360.0f;
        m_pitch = std::clamp(m_pitch + static_cast<float>(dy) * 0.55f, -85.0f, 85.0f);
        m_frameDirty = true;
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

BOOL CModelPreviewWnd::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/) {
    if (m_model.hasModel && zDelta != 0) {
        const float step = (zDelta > 0) ? 1.12f : (1.0f / 1.12f);
        AdjustZoom(step);
        return TRUE;
    }
    return FALSE;
}

void CModelPreviewWnd::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kSpinTimerId && m_autoSpin && !m_dragging && m_model.hasModel && IsWindowVisible()) {
        m_yaw = std::fmod(m_yaw + 0.9f, 360.0f);
        m_frameDirty = true;
        Invalidate(FALSE);
    }
    CWnd::OnTimer(nIDEvent);
}

} // namespace DNItemMaker::UI
