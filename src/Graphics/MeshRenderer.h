#pragma once

/**
 * @file MeshRenderer.h
 * @brief High-speed software 3D rasterizer featuring z-buffering, lighting, and viewport aids.
 *
 * Implements a pure CPU scanline rasterizer designed for interactive 3D model previewing
 * without external graphics API overhead. Features orthographic projection, directional
 * Gouraud and flat shading, texture mapping with alpha test, skeletal skinning, multi-object
 * scene composition (character + equipped items), a Blender-style 3D ground plane grid,
 * and an interactive XYZ orientation gizmo.
 */

#include "Graphics/AniParser.h"
#include "Graphics/DDSParser.h"
#include "Graphics/MeshParser.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Viewport shading and rasterization modes.
 */
enum class RenderMode : uint8_t {
    TexturedLit = 0, ///< Fully textured with directional diffuse lighting and alpha testing.
    ClayShaded = 1,  ///< Untextured clay material with ambient and directional lighting.
    Wireframe = 2    ///< Wireframe mesh outline mode.
};

/**
 * @brief Material binding applied to an individual submesh during rendering.
 */
struct SubmeshMaterial {
    std::shared_ptr<const ImageBuffer> diffuse;  ///< Decoded diffuse texture (or nullptr for clay shading).
    std::shared_ptr<const ImageBuffer> emissive; ///< Decoded emissive texture (or nullptr).
    float emissivePower = 1.0f;                  ///< Emissive intensity multiplier.
    bool alphaTest = true;                       ///< True to discard texels with alpha below threshold (0x7F).
};

/**
 * @brief Reusable memory buffers enabling zero-allocation per-frame rendering.
 */
struct RenderScratch {
    struct Vec3 {
        float x, y, z;
    };
    std::vector<float> zbuf;        ///< Depth buffer storing per-pixel floating-point depth.
    std::vector<Vec3> viewPos;      ///< Transformed vertex coordinates in view space.
    std::vector<Vec3> proj;         ///< Projected screen-space 2D coordinates with depth.
    std::vector<float> vtxShade;    ///< Per-vertex diffuse lighting factors.
};

/**
 * @brief Discrete renderable object in the scene (character body or equipped item).
 */
struct RenderObject {
    const ParsedMesh* mesh = nullptr;
    std::vector<SubmeshMaterial> materials;
    Matrix4 transform = Matrix4::Identity();
    const std::vector<Matrix4>* boneWorldMatrices = nullptr; ///< If animated / skinned
    const ParsedAnimationSet* skeleton = nullptr;            ///< Skeleton definition for bone lookups
};

/**
 * @class MeshRenderer
 * @brief Static software rasterizer for rendering 3D models into 32-bit RGBA framebuffers.
 */
class MeshRenderer {
public:
    /**
     * @brief Renders a composite scene with multiple objects (character body + equipped items).
     * @param objects List of discrete objects to render into the same depth buffer.
     * @param yawDegrees Camera orbit yaw in degrees.
     * @param pitchDegrees Camera orbit pitch in degrees.
     * @param width Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     * @param outImg Destination ImageBuffer receiving rendered RGBA pixels.
     * @param scratch Reusable scratch memory buffer.
     * @param zoom Camera zoom factor.
     * @param mode Shading and rasterization mode.
     * @param customCenter Optional camera focus point override.
     * @param customRadius Optional camera bounding radius override.
     */
    static void RenderSceneInto(std::span<const RenderObject> objects, float yawDegrees,
                                float pitchDegrees, uint32_t width, uint32_t height,
                                ImageBuffer& outImg, RenderScratch& scratch,
                                float zoom = 1.0f, RenderMode mode = RenderMode::TexturedLit,
                                const Vector3* customCenter = nullptr, float customRadius = 0.0f);

    /**
     * @brief Zero-allocation single-mesh rendering entry point into caller-supplied buffers.
     */
    static void RenderInto(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                           float pitchDegrees, uint32_t width, uint32_t height,
                           ImageBuffer& outImg, RenderScratch& scratch,
                           float zoom = 1.0f, RenderMode mode = RenderMode::TexturedLit);

    /**
     * @brief Renders a model into a newly allocated ImageBuffer with materials.
     */
    static ImageBuffer Render(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                              float pitchDegrees, uint32_t width, uint32_t height);

    /**
     * @brief Renders an untextured model into a newly allocated ImageBuffer.
     */
    static ImageBuffer Render(const ParsedMesh& mesh, float yawDegrees, float pitchDegrees, uint32_t width, uint32_t height) {
        return Render(mesh, {}, yawDegrees, pitchDegrees, width, height);
    }
};

} // namespace DNItemMaker::Graphics
