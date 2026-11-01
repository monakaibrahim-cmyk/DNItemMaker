#include "Graphics/MeshRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>

namespace DNItemMaker::Graphics {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

inline uint32_t PackPixelRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept {
    return static_cast<uint32_t>(r) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(a) << 24);
}

using P3 = RenderScratch::Vec3;

inline P3 Sub(const P3& a, const P3& b) noexcept {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline P3 Cross(const P3& a, const P3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

struct TextureView {
    const uint8_t* data = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t maskX = 0;
    uint32_t maskY = 0;
    bool potX = false;
    bool potY = false;

    explicit operator bool() const noexcept { return data != nullptr; }

    static TextureView From(const ImageBuffer* img) noexcept {
        if (!img || img->width == 0 || img->height == 0 || img->rgbaData.empty()) return {};
        TextureView tv;
        tv.data = img->rgbaData.data();
        tv.width = img->width;
        tv.height = img->height;
        tv.potX = (tv.width & (tv.width - 1)) == 0;
        tv.potY = (tv.height & (tv.height - 1)) == 0;
        tv.maskX = tv.potX ? (tv.width - 1) : 0;
        tv.maskY = tv.potY ? (tv.height - 1) : 0;
        return tv;
    }
};

inline std::array<uint32_t, 4> SampleBilinear(const TextureView& tex, float u, float v,
                                              bool alphaTest, bool& outDiscarded) noexcept {
    float uWrap = u - std::floor(u);
    float vWrap = v - std::floor(v);
    if (uWrap < 0.0f) uWrap += 1.0f;
    if (vWrap < 0.0f) vWrap += 1.0f;

    const float fx = uWrap * static_cast<float>(tex.width) - 0.5f;
    const float fy = vWrap * static_cast<float>(tex.height) - 0.5f;
    const float x0f = std::floor(fx);
    const float y0f = std::floor(fy);
    const uint32_t tx = std::min(256u, static_cast<uint32_t>((fx - x0f) * 256.0f));
    const uint32_t ty = std::min(256u, static_cast<uint32_t>((fy - y0f) * 256.0f));

    const int ix = static_cast<int>(x0f);
    const int iy = static_cast<int>(y0f);

    uint32_t x0, x1, y0, y1;
    if (tex.potX) {
        x0 = static_cast<uint32_t>(ix) & tex.maskX;
        x1 = static_cast<uint32_t>(ix + 1) & tex.maskX;
    } else {
        const int w = static_cast<int>(tex.width);
        const int wx = ((ix % w) + w) % w;
        x0 = static_cast<uint32_t>(wx);
        x1 = (x0 + 1u == tex.width) ? 0u : (x0 + 1u);
    }

    if (tex.potY) {
        y0 = static_cast<uint32_t>(iy) & tex.maskY;
        y1 = static_cast<uint32_t>(iy + 1) & tex.maskY;
    } else {
        const int h = static_cast<int>(tex.height);
        const int wy = ((iy % h) + h) % h;
        y0 = static_cast<uint32_t>(wy);
        y1 = (y0 + 1u == tex.height) ? 0u : (y0 + 1u);
    }

    const size_t row0 = static_cast<size_t>(y0) * tex.width;
    const size_t row1 = static_cast<size_t>(y1) * tex.width;
    const uint8_t* p00 = tex.data + (row0 + x0) * 4;
    const uint8_t* p10 = tex.data + (row0 + x1) * 4;
    const uint8_t* p01 = tex.data + (row1 + x0) * 4;
    const uint8_t* p11 = tex.data + (row1 + x1) * 4;

    const uint32_t invTx = 256u - tx;
    const uint32_t invTy = 256u - ty;
    const uint32_t w00 = invTx * invTy;
    const uint32_t w10 = tx * invTy;
    const uint32_t w01 = invTx * ty;
    const uint32_t w11 = tx * ty;

    const uint32_t a = (p00[3] * w00 + p10[3] * w10 + p01[3] * w01 + p11[3] * w11) >> 16;
    if (alphaTest && a < 128u) {
        outDiscarded = true;
        return {};
    }

    outDiscarded = false;
    return {
        (p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11) >> 16,
        (p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11) >> 16,
        (p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11) >> 16,
        a,
    };
}

inline uint8_t ToByte(float v) noexcept {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v);
}

inline void DrawClippedLine(uint32_t* pixels, int width, int height,
                            float fx0, float fy0, float fx1, float fy1, uint32_t color) noexcept {
    int x0 = static_cast<int>(std::lround(fx0));
    int y0 = static_cast<int>(std::lround(fy0));
    int x1 = static_cast<int>(std::lround(fx1));
    int y1 = static_cast<int>(std::lround(fy1));

    if ((x0 < 0 && x1 < 0) || (x0 >= width && x1 >= width) ||
        (y0 < 0 && y1 < 0) || (y0 >= height && y1 >= height)) {
        return;
    }

    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            pixels[y0 * width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void DrawClippedLine3D(uint32_t* pixels, float* zbuf, int width, int height,
                              float fx0, float fy0, float fz0,
                              float fx1, float fy1, float fz1, uint32_t color) noexcept {
    int x0 = static_cast<int>(std::lround(fx0));
    int y0 = static_cast<int>(std::lround(fy0));
    int x1 = static_cast<int>(std::lround(fx1));
    int y1 = static_cast<int>(std::lround(fy1));

    if ((x0 < 0 && x1 < 0) || (x0 >= width && x1 >= width) ||
        (y0 < 0 && y1 < 0) || (y0 >= height && y1 >= height)) {
        return;
    }

    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    const float totalDist = std::max(1.0f, std::hypot(static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)));
    const int startX = x0, startY = y0;

    while (true) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            const float curDist = std::hypot(static_cast<float>(x0 - startX), static_cast<float>(y0 - startY));
            const float t = std::clamp(curDist / totalDist, 0.0f, 1.0f);
            const float z = fz0 + (fz1 - fz0) * t;
            const size_t idx = static_cast<size_t>(y0) * width + x0;
            if (z < zbuf[idx]) {
                zbuf[idx] = z;
                pixels[idx] = color;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void DrawFilledCircle(uint32_t* pixels, int width, int height, int cx, int cy, int r, uint32_t color) noexcept {
    const int r2 = r * r;
    const int yMin = std::max(0, cy - r);
    const int yMax = std::min(height - 1, cy + r);
    for (int y = yMin; y <= yMax; ++y) {
        const int dy = y - cy;
        const int dxLimit = static_cast<int>(std::sqrt(std::max(0, r2 - dy * dy)));
        const int xMin = std::max(0, cx - dxLimit);
        const int xMax = std::min(width - 1, cx + dxLimit);
        std::fill_n(pixels + y * width + xMin, xMax - xMin + 1, color);
    }
}

inline void DrawLetter(uint32_t* pixels, int width, int height, int cx, int cy, char ch, uint32_t color) noexcept {
    static const uint8_t kX[5] = { 0b10001, 0b01010, 0b00100, 0b01010, 0b10001 };
    static const uint8_t kY[5] = { 0b10001, 0b01010, 0b00100, 0b00100, 0b00100 };
    static const uint8_t kZ[5] = { 0b11111, 0b00010, 0b00100, 0b01000, 0b11111 };
    const uint8_t* bmp = (ch == 'X') ? kX : ((ch == 'Y') ? kY : ((ch == 'Z') ? kZ : nullptr));
    if (!bmp) return;
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            if ((bmp[r] >> (4 - c)) & 1) {
                const int px = cx - 2 + c;
                const int py = cy - 2 + r;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    pixels[py * width + px] = color;
                }
            }
        }
    }
}

} // namespace

void MeshRenderer::RenderSceneInto(std::span<const RenderObject> objects, float yawDegrees,
                                   float pitchDegrees, uint32_t width, uint32_t height,
                                   ImageBuffer& img, RenderScratch& scratch,
                                   float zoom, RenderMode mode,
                                   const Vector3* customCenter, float customRadius) {
    img.width = width;
    img.height = height;
    const size_t totalPixels = static_cast<size_t>(width) * height;
    img.rgbaData.resize(totalPixels * 4);
    if (totalPixels == 0) return;

    auto* pixels = reinterpret_cast<uint32_t*>(img.rgbaData.data());

    // Background vertical gradient fill
    const float invH = 1.0f / static_cast<float>(std::max(1u, height - 1));
    for (uint32_t y = 0; y < height; ++y) {
        const float t = static_cast<float>(y) * invH;
        const auto r = static_cast<uint8_t>(18.0f + 16.0f * t);
        const auto g = static_cast<uint8_t>(22.0f + 18.0f * t);
        const auto b = static_cast<uint8_t>(32.0f + 22.0f * t);
        std::fill_n(pixels + static_cast<size_t>(y) * width, width, PackPixelRGBA(r, g, b));
    }
    if (scratch.zbuf.size() != totalPixels) {
        scratch.zbuf.assign(totalPixels, std::numeric_limits<float>::max());
    } else {
        std::fill(scratch.zbuf.begin(), scratch.zbuf.end(), std::numeric_limits<float>::max());
    }
    float* zbuf = scratch.zbuf.data();

    // Compute bounding center and radius
    Vector3 c{ 0.0f, 0.0f, 0.0f };
    Vector3 ext{ 100.0f, 100.0f, 100.0f };
    float radius = 50.0f;
    bool hasValidMesh = false;

    if (customCenter && customRadius > 0.0f) {
        c = *customCenter;
        radius = customRadius;
        hasValidMesh = true;
    } else {
        Vector3 bmin{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        Vector3 bmax{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

        for (const auto& obj : objects) {
            if (obj.mesh && obj.mesh->valid && !obj.mesh->submeshes.empty()) {
                bmin.x = std::min(bmin.x, obj.mesh->bbox.min.x);
                bmin.y = std::min(bmin.y, obj.mesh->bbox.min.y);
                bmin.z = std::min(bmin.z, obj.mesh->bbox.min.z);
                bmax.x = std::max(bmax.x, obj.mesh->bbox.max.x);
                bmax.y = std::max(bmax.y, obj.mesh->bbox.max.y);
                bmax.z = std::max(bmax.z, obj.mesh->bbox.max.z);
                hasValidMesh = true;
            }
        }
        if (hasValidMesh) {
            c = { (bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f, (bmin.z + bmax.z) * 0.5f };
            ext = { bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z };
            radius = 0.5f * std::sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);
            if (radius <= 1e-4f) radius = 50.0f;
        }
    }

    const float clampedZoom = std::clamp(zoom, 0.25f, 5.0f);
    const float yaw = yawDegrees * (kPi / 180.0f);
    const float pitch = pitchDegrees * (kPi / 180.0f);
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float scale = (0.46f * clampedZoom) * static_cast<float>(std::min(width, height)) / radius;
    const float halfW = static_cast<float>(width) * 0.5f;
    const float halfH = static_cast<float>(height) * 0.5f;
    const int iw = static_cast<int>(width);
    const int ih = static_cast<int>(height);

    // 1. Render 3D Ground Plane Grid (Blender-style 3D Tiles)
    const float groundY = hasValidMesh ? (c.y - ext.y * 0.5f) : 0.0f;
    const float gridExtent = radius * 1.6f;
    const int gridDivs = 16;
    const float step = (gridExtent * 2.0f) / static_cast<float>(gridDivs);

    auto projectPt = [&](float x, float y, float z) -> P3 {
        const float rx = x - c.x;
        const float ry = y - c.y;
        const float rz = z - c.z;
        const float x1 = rx * cy + rz * sy;
        const float z1 = -rx * sy + rz * cy;
        const float y2 = ry * cp - z1 * sp;
        const float z2 = ry * sp + z1 * cp;
        return P3{ halfW + x1 * scale, halfH - y2 * scale, z2 };
    };

    for (int i = 0; i <= gridDivs; ++i) {
        const float gz = -gridExtent + static_cast<float>(i) * step;
        const P3 p0 = projectPt(c.x - gridExtent, groundY, c.z + gz);
        const P3 p1 = projectPt(c.x + gridExtent, groundY, c.z + gz);
        const uint32_t clr = (i == gridDivs / 2) ? PackPixelRGBA(210, 60, 60) : PackPixelRGBA(44, 52, 70);
        DrawClippedLine3D(pixels, zbuf, iw, ih, p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, clr);
    }
    for (int i = 0; i <= gridDivs; ++i) {
        const float gx = -gridExtent + static_cast<float>(i) * step;
        const P3 p0 = projectPt(c.x + gx, groundY, c.z - gridExtent);
        const P3 p1 = projectPt(c.x + gx, groundY, c.z + gridExtent);
        const uint32_t clr = (i == gridDivs / 2) ? PackPixelRGBA(60, 130, 240) : PackPixelRGBA(44, 52, 70);
        DrawClippedLine3D(pixels, zbuf, iw, ih, p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, clr);
    }

    // View-space key light
    static const P3 kLight = [] {
        P3 l{ -0.35f, 0.55f, -0.75f };
        const float n = std::sqrt(l.x * l.x + l.y * l.y + l.z * l.z);
        return P3{ l.x / n, l.y / n, l.z / n };
    }();

    // 2. Render all scene objects
    for (const auto& obj : objects) {
        if (!obj.mesh || !obj.mesh->valid) continue;
        const auto& mesh = *obj.mesh;

        for (size_t s = 0; s < mesh.submeshes.size(); ++s) {
            const auto& sm = mesh.submeshes[s];
            const size_t vCount = sm.positions.size();
            if (vCount == 0 || sm.triangles.empty()) continue;

            const SubmeshMaterial* mat = s < obj.materials.size() ? &obj.materials[s] : nullptr;
            const bool hasUvs = sm.uvs.size() == vCount;
            const bool allowTex = (mode == RenderMode::TexturedLit);
            const TextureView tex = (allowTex && mat && hasUvs) ? TextureView::From(mat->diffuse.get()) : TextureView{};
            const TextureView emi = (tex && mat) ? TextureView::From(mat->emissive.get()) : TextureView{};
            const float emiPower = mat ? std::max(0.0f, mat->emissivePower) : 1.0f;
            const bool alphaTest = static_cast<bool>(tex) && mat->alphaTest;
            const bool hasNormals = sm.normals.size() == vCount;

            auto shadeFromLambert = [hasTex = static_cast<bool>(tex)](float lambert) noexcept {
                return hasTex ? (0.48f + 0.62f * lambert) : (0.28f + 0.72f * lambert);
            };

            scratch.viewPos.resize(vCount);
            scratch.proj.resize(vCount);
            scratch.vtxShade.assign(vCount, -1.0f);

            P3* viewPos = scratch.viewPos.data();
            P3* proj = scratch.proj.data();
            float* vtxShade = scratch.vtxShade.data();

            // Check if this submesh uses skeletal skinning
            const bool useSkinning = (obj.boneWorldMatrices != nullptr && sm.usesPhysique &&
                                      !sm.skinWeights.empty() && obj.skeleton != nullptr);

            std::vector<Matrix4> submeshSkinMatrices;
            if (useSkinning) {
                submeshSkinMatrices.resize(sm.linkedBones.size(), Matrix4::Identity());
                for (size_t b = 0; b < sm.linkedBones.size(); ++b) {
                    const std::string& bName = sm.linkedBones[b];
                    // Find inverse world matrix in mesh bones
                    Matrix4 invMat = Matrix4::Identity();
                    for (const auto& mb : mesh.bones) {
                        if (mb.name == bName) {
                            std::memcpy(invMat.m, mb.invWorldMatrix, sizeof(invMat.m));
                            break;
                        }
                    }
                    // Find skeleton bone matrix
                    const int32_t skelIdx = obj.skeleton->FindBoneIndex(bName);
                    if (skelIdx >= 0 && static_cast<size_t>(skelIdx) < obj.boneWorldMatrices->size()) {
                        const Matrix4& boneMat = (*obj.boneWorldMatrices)[static_cast<size_t>(skelIdx)];
                        submeshSkinMatrices[b] = Matrix4::Multiply(invMat, boneMat);
                    }
                }
            }

            // Determine fallback parent transform
            Matrix4 fallbackMat = obj.transform;
            if (!useSkinning && obj.boneWorldMatrices && !sm.parentName.empty() && obj.skeleton) {
                const int32_t pIdx = obj.skeleton->FindBoneIndex(sm.parentName);
                if (pIdx >= 0 && static_cast<size_t>(pIdx) < obj.boneWorldMatrices->size()) {
                    fallbackMat = (*obj.boneWorldMatrices)[static_cast<size_t>(pIdx)];
                }
            }

            for (size_t i = 0; i < vCount; ++i) {
                Vector3 worldP = sm.positions[i];
                Vector3 worldN = hasNormals ? sm.normals[i] : Vector3{ 0.0f, 1.0f, 0.0f };

                if (useSkinning) {
                    const auto& sw = sm.skinWeights[i];
                    Vector3 sp{ 0.0f, 0.0f, 0.0f };
                    Vector3 sn{ 0.0f, 0.0f, 0.0f };
                    for (int k = 0; k < 4; ++k) {
                        const float w = sw.weights[k];
                        if (w <= 0.0f) continue;
                        const uint16_t bIdx = sw.boneIndices[k];
                        if (bIdx < submeshSkinMatrices.size()) {
                            const auto& M = submeshSkinMatrices[bIdx];
                            Vector3 tp = M.TransformPoint(worldP);
                            Vector3 tn = M.TransformVector(worldN);
                            sp.x += tp.x * w; sp.y += tp.y * w; sp.z += tp.z * w;
                            sn.x += tn.x * w; sn.y += tn.y * w; sn.z += tn.z * w;
                        }
                    }
                    worldP = sp;
                    worldN = sn;
                } else {
                    worldP = fallbackMat.TransformPoint(worldP);
                    worldN = fallbackMat.TransformVector(worldN);
                }

                // Project to view space
                const float rx = worldP.x - c.x;
                const float ry = worldP.y - c.y;
                const float rz = worldP.z - c.z;
                const float x1 = rx * cy + rz * sy;
                const float z1 = -rx * sy + rz * cy;
                const float y2 = ry * cp - z1 * sp;
                const float z2 = ry * sp + z1 * cp;
                viewPos[i] = { x1, y2, z2 };
                proj[i] = { halfW + x1 * scale, halfH - y2 * scale, z2 };

                if (hasNormals) {
                    const float nlSq = worldN.x * worldN.x + worldN.y * worldN.y + worldN.z * worldN.z;
                    if (nlSq > 1e-8f) {
                        const float invNl = 1.0f / std::sqrt(nlSq);
                        const float nx = worldN.x * invNl, ny = worldN.y * invNl, nz = worldN.z * invNl;
                        const float nx1 = nx * cy + nz * sy;
                        const float nz1 = -nx * sy + nz * cy;
                        const float ny2 = ny * cp - nz1 * sp;
                        const float nz2 = ny * sp + nz1 * cp;
                        vtxShade[i] = shadeFromLambert(std::fabs(nx1 * kLight.x + ny2 * kLight.y + nz2 * kLight.z));
                    }
                }
            }

            if (mode == RenderMode::Wireframe) {
                const uint32_t wireColor = PackPixelRGBA(90, 200, 255);
                for (size_t t = 0; t + 2 < sm.triangles.size(); t += 3) {
                    const uint32_t ia = sm.triangles[t], ib = sm.triangles[t + 1], id = sm.triangles[t + 2];
                    if (ia >= vCount || ib >= vCount || id >= vCount) continue;
                    const P3& a = proj[ia];
                    const P3& b = proj[ib];
                    const P3& d = proj[id];
                    DrawClippedLine(pixels, iw, ih, a.x, a.y, b.x, b.y, wireColor);
                    DrawClippedLine(pixels, iw, ih, b.x, b.y, d.x, d.y, wireColor);
                    DrawClippedLine(pixels, iw, ih, d.x, d.y, a.x, a.y, wireColor);
                }
                continue;
            }

            for (size_t t = 0; t + 2 < sm.triangles.size(); t += 3) {
                const uint32_t ia = sm.triangles[t], ib = sm.triangles[t + 1], id = sm.triangles[t + 2];
                if (ia >= vCount || ib >= vCount || id >= vCount) continue;
                const P3& a = proj[ia];
                const P3& b = proj[ib];
                const P3& d = proj[id];

                const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, d.x }))));
                const int x1 = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, d.x }))));
                const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, d.y }))));
                const int y1 = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, d.y }))));
                if (x0 > x1 || y0 > y1) continue;

                const float area = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
                if (std::fabs(area) < 1e-6f) continue;
                const float inv = 1.0f / area;

                float sA = vtxShade[ia], sB = vtxShade[ib], sD = vtxShade[id];
                if (sA < 0.0f || sB < 0.0f || sD < 0.0f) {
                    P3 n = Cross(Sub(viewPos[ib], viewPos[ia]), Sub(viewPos[id], viewPos[ia]));
                    const float lenSq = n.x * n.x + n.y * n.y + n.z * n.z;
                    if (lenSq < 1e-16f) continue;
                    const float invLen = 1.0f / std::sqrt(lenSq);
                    const float flat = shadeFromLambert(std::fabs((n.x * kLight.x + n.y * kLight.y + n.z * kLight.z) * invLen));
                    sA = sB = sD = flat;
                }

                const float px0 = static_cast<float>(x0) + 0.5f;
                const float py0 = static_cast<float>(y0) + 0.5f;
                float w0Row = ((b.x - px0) * (d.y - py0) - (b.y - py0) * (d.x - px0)) * inv;
                float w1Row = ((d.x - px0) * (a.y - py0) - (d.y - py0) * (a.x - px0)) * inv;
                const float dw0Dx = (b.y - d.y) * inv;
                const float dw0Dy = (d.x - b.x) * inv;
                const float dw1Dx = (d.y - a.y) * inv;
                const float dw1Dy = (a.x - d.x) * inv;

                const Vector2 uvA = tex ? sm.uvs[ia] : Vector2{};
                const Vector2 uvB = tex ? sm.uvs[ib] : Vector2{};
                const Vector2 uvD = tex ? sm.uvs[id] : Vector2{};

                for (int y = y0; y <= y1; ++y) {
                    float w0 = w0Row;
                    float w1 = w1Row;
                    const size_t rowOffset = static_cast<size_t>(y) * width;

                    for (int x = x0; x <= x1; ++x) {
                        const float w2 = 1.0f - w0 - w1;
                        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                            const float z = w0 * a.z + w1 * b.z + w2 * d.z;
                            const size_t pIdx = rowOffset + x;
                            if (z < zbuf[pIdx]) {
                                const float shade = w0 * sA + w1 * sB + w2 * sD;

                                if (tex) {
                                    const float u = w0 * uvA.x + w1 * uvB.x + w2 * uvD.x;
                                    const float v = w0 * uvA.y + w1 * uvB.y + w2 * uvD.y;
                                    bool discarded = false;
                                    const auto samp = SampleBilinear(tex, u, v, alphaTest, discarded);
                                    if (!discarded) {
                                        zbuf[pIdx] = z;
                                        float r = static_cast<float>(samp[0]) * shade;
                                        float g = static_cast<float>(samp[1]) * shade;
                                        float bCol = static_cast<float>(samp[2]) * shade;

                                        if (emi) {
                                            bool emiDiscarded = false;
                                            const auto eSamp = SampleBilinear(emi, u, v, false, emiDiscarded);
                                            r += static_cast<float>(eSamp[0]) * emiPower;
                                            g += static_cast<float>(eSamp[1]) * emiPower;
                                            bCol += static_cast<float>(eSamp[2]) * emiPower;
                                        }

                                        pixels[pIdx] = PackPixelRGBA(ToByte(r), ToByte(g), ToByte(bCol));
                                    }
                                } else {
                                    zbuf[pIdx] = z;
                                    const auto cVal = ToByte(shade * 215.0f);
                                    pixels[pIdx] = PackPixelRGBA(cVal, cVal, cVal);
                                }
                            }
                        }
                        w0 += dw0Dx;
                        w1 += dw1Dx;
                    }
                    w0Row += dw0Dy;
                    w1Row += dw1Dy;
                }
            }
        }
    }

    // 3. Interactive XYZ Orientation Gizmo
    {
        const float gizmoRadius = 24.0f;
        const float gizmoMargin = 38.0f;
        const float gizmoX = static_cast<float>(width) - gizmoMargin;
        const float gizmoY = gizmoMargin;

        struct AxisDef {
            char label;
            float x, y, z;
            uint32_t color;
        };

        const AxisDef axes[3] = {
            { 'X', 1.0f, 0.0f, 0.0f, PackPixelRGBA(235, 60, 60) },
            { 'Y', 0.0f, 1.0f, 0.0f, PackPixelRGBA(60, 210, 60) },
            { 'Z', 0.0f, 0.0f, 1.0f, PackPixelRGBA(60, 140, 245) }
        };

        struct ProjectedAxis {
            char label;
            float px, py, depth;
            uint32_t color;
        };
        std::vector<ProjectedAxis> projAxes;
        for (const auto& a : axes) {
            const float x1 = a.x * cy + a.z * sy;
            const float z1 = -a.x * sy + a.z * cy;
            const float y2 = a.y * cp - z1 * sp;
            const float z2 = a.y * sp + z1 * cp;
            projAxes.push_back({ a.label, gizmoX + x1 * gizmoRadius, gizmoY - y2 * gizmoRadius, z2, a.color });

            DrawClippedLine(pixels, iw, ih, gizmoX, gizmoY,
                            gizmoX - x1 * gizmoRadius * 0.65f, gizmoY + y2 * gizmoRadius * 0.65f,
                            PackPixelRGBA(55, 62, 78));
        }

        std::sort(projAxes.begin(), projAxes.end(), [](const auto& l, const auto& r) {
            return l.depth < r.depth;
        });

        DrawFilledCircle(pixels, iw, ih, static_cast<int>(gizmoX), static_cast<int>(gizmoY), 4, PackPixelRGBA(80, 90, 110));

        for (const auto& pa : projAxes) {
            DrawClippedLine(pixels, iw, ih, gizmoX, gizmoY, pa.px, pa.py, pa.color);
            const int capX = static_cast<int>(std::lround(pa.px));
            const int capY = static_cast<int>(std::lround(pa.py));
            DrawFilledCircle(pixels, iw, ih, capX, capY, 7, pa.color);
            DrawLetter(pixels, iw, ih, capX, capY, pa.label, PackPixelRGBA(255, 255, 255));
        }
    }
}

void MeshRenderer::RenderInto(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                              float pitchDegrees, uint32_t width, uint32_t height,
                              ImageBuffer& outImg, RenderScratch& scratch,
                              float zoom, RenderMode mode) {
    RenderObject obj{ &mesh, materials };
    RenderSceneInto(std::span<const RenderObject>(&obj, 1), yawDegrees, pitchDegrees, width, height, outImg, scratch, zoom, mode);
}

ImageBuffer MeshRenderer::Render(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                                 float pitchDegrees, uint32_t width, uint32_t height) {
    ImageBuffer img;
    RenderScratch scratch;
    RenderInto(mesh, materials, yawDegrees, pitchDegrees, width, height, img, scratch);
    return img;
}

} // namespace DNItemMaker::Graphics
