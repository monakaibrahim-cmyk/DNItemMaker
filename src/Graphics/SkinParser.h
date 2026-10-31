#pragma once

/**
 * @file SkinParser.h
 * @brief Binary parser for Eternity Engine skin files (.skn).
 *
 * Implements binary parsing for Dragon Nest .skn files, which associate a 3D geometry
 * mesh (.msh) with material shaders (.fx), diffuse textures, emissive lighting parameters,
 * and alpha blending flags.
 */

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Material parameters and texture bindings applied to one submesh of a 3D model.
 */
struct SubSkin {
    std::string materialName;       ///< Material slot name in the modeling package.
    std::string effectName;         ///< Engine HLSL effect shader name (e.g. "Diffuse.fx", "Item_Alpha.fx").
    float alphaValue = 1.0f;        ///< Material overall opacity factor (0.0 to 1.0).
    bool alphaBlend = false;        ///< Enables alpha transparency blending when true.

    std::string diffuseTexture;     ///< Filename of the primary diffuse DDS texture (g_DiffuseTex).
    std::string emissiveTexture;    ///< Filename of the self-illumination emissive texture (g_EmissiveTex).
    float emissivePower = 1.0f;     ///< Emissive intensity multiplier (g_EmissivePower).
    std::array<float, 4> diffuseColor{ 1.0f, 1.0f, 1.0f, 1.0f }; ///< RGBA material tint color.
};

/**
 * @brief Fully parsed Eternity Engine skin definition.
 */
struct ParsedSkin {
    bool valid = false;             ///< True if header and mesh name were cleanly parsed.
    bool complete = false;          ///< True if all declared submesh skin blocks parsed without truncation.
    std::string errorMessage;       ///< Diagnostic error description if parsing failed.
    std::filesystem::path filePath; ///< Source file path on disk.
    int32_t version = 0;            ///< Skin file format version.
    std::string meshName;           ///< Target .msh geometry mesh filename referenced by this skin.
    std::vector<SubSkin> subSkins;  ///< Ordered list of material slots matching the mesh's submeshes.
};

/**
 * @class SkinParser
 * @brief Static parser for loading Eternity Engine .skn files.
 */
class SkinParser {
public:
    /**
     * @brief Loads and parses a .skn skin file from disk.
     * @param path Filesystem path to the .skn file.
     * @return ParsedSkin structure containing material bindings or error status.
     */
    static ParsedSkin LoadFromFile(const std::filesystem::path& path);

    /**
     * @brief Loads and parses a .skn skin file directly from a memory buffer.
     * @param data Pointer to raw file bytes.
     * @param size Length of memory buffer in bytes.
     * @return ParsedSkin structure containing material bindings or error status.
     */
    static ParsedSkin LoadFromMemory(const uint8_t* data, size_t size);
};

} // namespace DNItemMaker::Graphics
