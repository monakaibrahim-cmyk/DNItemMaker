#pragma once

/**
 * @file MeshRenderer.h
 * @brief High-speed software 3D rasterizer featuring z-buffering, lighting, and viewport aids.
 *
 * Implements a pure CPU scanline rasterizer designed for interactive 3D model previewing
 * without external graphics API overhead. Features orthographic projection, directional
 * Gouraud and flat shading, texture mapping with alpha test, a Blender-style 3D ground plane
 * grid, and an interactive XYZ orientation gizmo.
 */

#include "DDSParser.h"
#include "MeshParser.h"

#include <cstdint>
#include <memory>
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
 * @class MeshRenderer
 * @brief Static software rasterizer for rendering 3D models into 32-bit RGBA framebuffers.
 */
class MeshRenderer {
public:
    /**
     * @brief Zero-allocation rendering entry point into caller-supplied image and scratch buffers.
     * @param mesh Parsed 3D geometry to render.
     * @param materials List of submesh material bindings.
     * @param yawDegrees Orbit rotation angle around vertical Y axis in degrees.
     * @param pitchDegrees Orbit elevation angle around lateral X axis in degrees.
     * @param width Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     * @param outImg Destination ImageBuffer receiving rendered RGBA pixels.
     * @param scratch Reusable scratch memory buffer.
     * @param zoom Camera zoom magnification factor (default 1.0).
     * @param mode Shading and rasterization mode.
     */
    static void RenderInto(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                           float pitchDegrees, uint32_t width, uint32_t height,
                           ImageBuffer& outImg, RenderScratch& scratch,
                           float zoom = 1.0f, RenderMode mode = RenderMode::TexturedLit);

    /**
     * @brief Renders a model into a newly allocated ImageBuffer with materials.
     * @param mesh Parsed 3D geometry to render.
     * @param materials List of submesh material bindings.
     * @param yawDegrees Orbit rotation angle in degrees.
     * @param pitchDegrees Orbit elevation angle in degrees.
     * @param width Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     * @return Newly allocated ImageBuffer with rendered scene.
     */
    static ImageBuffer Render(const ParsedMesh& mesh, const std::vector<SubmeshMaterial>& materials, float yawDegrees,
                              float pitchDegrees, uint32_t width, uint32_t height);

    /**
     * @brief Renders an untextured model into a newly allocated ImageBuffer.
     * @param mesh Parsed 3D geometry to render.
     * @param yawDegrees Orbit rotation angle in degrees.
     * @param pitchDegrees Orbit elevation angle in degrees.
     * @param width Framebuffer width in pixels.
     * @param height Framebuffer height in pixels.
     * @return Newly allocated ImageBuffer with rendered scene.
     */
    static ImageBuffer Render(const ParsedMesh& mesh, float yawDegrees, float pitchDegrees, uint32_t width, uint32_t height) {
        return Render(mesh, {}, yawDegrees, pitchDegrees, width, height);
    }
};

} // namespace DNItemMaker::Graphics
