#pragma once

/**
 * @file DDSParser.h
 * @brief High-performance software decoder for DirectDraw Surface (.dds) textures.
 *
 * Provides pure software decompression for compressed block formats (DXT1/BC1, DXT5/BC3)
 * and uncompressed 32-bit RGBA formats without requiring DirectX, Direct3D, or GPU runtime
 * dependencies. Used for decoding Dragon Nest UI icon atlases and 3D model diffuse/emissive textures.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief In-memory raw 32-bit RGBA pixel buffer.
 */
struct ImageBuffer {
    uint32_t width = 0;             ///< Image width in pixels.
    uint32_t height = 0;            ///< Image height in pixels.
    std::vector<uint8_t> rgbaData;  ///< Contiguous 32-bit RGBA pixel data (size = width * height * 4).
};

/**
 * @class DDSParser
 * @brief Static utility class for decoding DDS files and extracting atlas sub-rectangles.
 */
class DDSParser {
public:
    DDSParser() = default;
    ~DDSParser() = default;

    /**
     * @brief Decodes a .dds file from disk into a 32-bit RGBA pixel buffer.
     * @param filePath Filesystem path to the .dds file.
     * @param outImage Destination ImageBuffer receiving decoded pixels.
     * @return true on success, false on unsupported format or I/O failure.
     */
    static bool DecodeFile(const std::filesystem::path& filePath, ImageBuffer& outImage);

    /**
     * @brief Decodes a .dds file directly from a raw memory buffer.
     * @param data Pointer to DDS file memory bytes.
     * @param size Length of memory buffer in bytes.
     * @param outImage Destination ImageBuffer receiving decoded pixels.
     * @return true on success, false on invalid header or corrupt data.
     */
    static bool DecodeMemory(const uint8_t* data, size_t size, ImageBuffer& outImage);

    /**
     * @brief Crops a single 2D icon tile from an atlas grid by its 0-based index.
     * @param atlas Decoded source atlas image buffer.
     * @param tileIndex 0-based icon tile index within the grid.
     * @param tileWidth Width in pixels of each icon cell (typically 50px).
     * @param tileHeight Height in pixels of each icon cell (typically 50px).
     * @param outTile Destination ImageBuffer receiving cropped icon pixels.
     * @return true if successfully extracted, false if tile index is out of bounds.
     */
    static bool CropIconTile(const ImageBuffer& atlas, int tileIndex, int tileWidth, int tileHeight, ImageBuffer& outTile);

    /**
     * @brief Encodes a 32-bit RGBA image into an uncompressed 32-bit DDS file in memory.
     * @param image Source ImageBuffer.
     * @param outDdsData Output buffer receiving raw DDS file bytes.
     * @return true on success.
     */
    static bool EncodeRGBA32(const ImageBuffer& image, std::vector<uint8_t>& outDdsData);

    /**
     * @brief Saves a 32-bit RGBA image to a .dds file on disk.
     * @param filePath Filesystem path for destination .dds.
     * @param image Source ImageBuffer.
     * @return true on success.
     */
    static bool SaveFileRGBA32(const std::filesystem::path& filePath, const ImageBuffer& image);

    /**
     * @brief Scales an image buffer to destination dimensions using bilinear interpolation.
     * @param src Source ImageBuffer.
     * @param dstWidth Target width in pixels.
     * @param dstHeight Target height in pixels.
     * @param outScaled Destination ImageBuffer.
     * @return true on success.
     */
    static bool ScaleImageBilinear(const ImageBuffer& src, uint32_t dstWidth, uint32_t dstHeight, ImageBuffer& outScaled);

    /**
     * @brief Overlays a glowing Dragon Nest rank border onto an icon (50x50 pixels).
     * @param icon Icon buffer to modify in-place.
     * @param rank Item rank (0=Normal, 1=Magic, 2=Rare, 3=Epic, 4=Unique, 5=Legend, 6=Divine).
     */
    static void ApplyRankBorder(ImageBuffer& icon, int rank);

private:
    static void DecompressDXT1Block(const uint8_t* block, uint32_t* dest, uint32_t stride);
    static void DecompressDXT5Block(const uint8_t* block, uint32_t* dest, uint32_t stride);
};

} // namespace DNItemMaker::Graphics
