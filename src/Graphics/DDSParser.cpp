#include "DDSParser.h"
#include <Common/Logs/Logs.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>

namespace DNItemMaker::Graphics
{
#pragma pack(push, 1)
struct DDS_PIXELFORMAT
{
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER
{
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};
#pragma pack(pop)

constexpr uint32_t FOURCC_DXT1 = 0x31545844; // 'DXT1'
constexpr uint32_t FOURCC_DXT3 = 0x33545844; // 'DXT3'
constexpr uint32_t FOURCC_DXT5 = 0x35545844; // 'DXT5'

static constexpr uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept
{
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

static constexpr uint8_t Expand5To8(uint32_t v) noexcept
{
    return static_cast<uint8_t>((v << 3) | (v >> 2));
}

static constexpr uint8_t Expand6To8(uint32_t v) noexcept
{
    return static_cast<uint8_t>((v << 2) | (v >> 4));
}

void DDSParser::DecompressDXT1Block(const uint8_t* block, uint32_t* dest, uint32_t stride)
{
    uint16_t color0 = 0;
    uint16_t color1 = 0;
    uint32_t code = 0;
    std::memcpy(&color0, block, sizeof(uint16_t));
    std::memcpy(&color1, block + 2, sizeof(uint16_t));
    std::memcpy(&code, block + 4, sizeof(uint32_t));

    const uint8_t r0 = Expand5To8((color0 >> 11) & 0x1Fu);
    const uint8_t g0 = Expand6To8((color0 >> 5) & 0x3Fu);
    const uint8_t b0 = Expand5To8(color0 & 0x1Fu);

    const uint8_t r1 = Expand5To8((color1 >> 11) & 0x1Fu);
    const uint8_t g1 = Expand6To8((color1 >> 5) & 0x3Fu);
    const uint8_t b1 = Expand5To8(color1 & 0x1Fu);

    uint32_t colors[4];
    colors[0] = PackRGBA(r0, g0, b0, 255);
    colors[1] = PackRGBA(r1, g1, b1, 255);

    if (color0 > color1)
    {
        colors[2] = PackRGBA(static_cast<uint8_t>((2u * r0 + r1) / 3u),
                             static_cast<uint8_t>((2u * g0 + g1) / 3u),
                             static_cast<uint8_t>((2u * b0 + b1) / 3u), 255);
        colors[3] = PackRGBA(static_cast<uint8_t>((r0 + 2u * r1) / 3u),
                             static_cast<uint8_t>((g0 + 2u * g1) / 3u),
                             static_cast<uint8_t>((b0 + 2u * b1) / 3u), 255);
    }
    else
    {
        colors[2] = PackRGBA(static_cast<uint8_t>((uint32_t(r0) + r1) >> 1),
                             static_cast<uint8_t>((uint32_t(g0) + g1) >> 1),
                             static_cast<uint8_t>((uint32_t(b0) + b1) >> 1), 255);
        colors[3] = 0u; // Transparent black
    }

    for (uint32_t y = 0; y < 4; ++y)
    {
        uint32_t* row = dest + y * stride;
        row[0] = colors[code & 0x03u];
        row[1] = colors[(code >> 2) & 0x03u];
        row[2] = colors[(code >> 4) & 0x03u];
        row[3] = colors[(code >> 6) & 0x03u];
        code >>= 8;
    }
}

void DDSParser::DecompressDXT5Block(const uint8_t* block, uint32_t* dest, uint32_t stride)
{
    // 1. Alpha channel decompression
    const uint32_t alpha0 = block[0];
    const uint32_t alpha1 = block[1];
    uint8_t alphas[8];
    alphas[0] = static_cast<uint8_t>(alpha0);
    alphas[1] = static_cast<uint8_t>(alpha1);

    if (alpha0 > alpha1)
    {
        alphas[2] = static_cast<uint8_t>((6u * alpha0 + 1u * alpha1) / 7u);
        alphas[3] = static_cast<uint8_t>((5u * alpha0 + 2u * alpha1) / 7u);
        alphas[4] = static_cast<uint8_t>((4u * alpha0 + 3u * alpha1) / 7u);
        alphas[5] = static_cast<uint8_t>((3u * alpha0 + 4u * alpha1) / 7u);
        alphas[6] = static_cast<uint8_t>((2u * alpha0 + 5u * alpha1) / 7u);
        alphas[7] = static_cast<uint8_t>((1u * alpha0 + 6u * alpha1) / 7u);
    }
    else
    {
        alphas[2] = static_cast<uint8_t>((4u * alpha0 + 1u * alpha1) / 5u);
        alphas[3] = static_cast<uint8_t>((3u * alpha0 + 2u * alpha1) / 5u);
        alphas[4] = static_cast<uint8_t>((2u * alpha0 + 3u * alpha1) / 5u);
        alphas[5] = static_cast<uint8_t>((1u * alpha0 + 4u * alpha1) / 5u);
        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t alphaIndices = 0;
    std::memcpy(&alphaIndices, block + 2, 6);

    // 2. Color channel decompression
    const uint8_t* colorBlock = block + 8;
    uint16_t color0 = 0;
    uint16_t color1 = 0;
    uint32_t colorCode = 0;
    std::memcpy(&color0, colorBlock, sizeof(uint16_t));
    std::memcpy(&color1, colorBlock + 2, sizeof(uint16_t));
    std::memcpy(&colorCode, colorBlock + 4, sizeof(uint32_t));

    const uint8_t r0 = Expand5To8((color0 >> 11) & 0x1Fu);
    const uint8_t g0 = Expand6To8((color0 >> 5) & 0x3Fu);
    const uint8_t b0 = Expand5To8(color0 & 0x1Fu);

    const uint8_t r1 = Expand5To8((color1 >> 11) & 0x1Fu);
    const uint8_t g1 = Expand6To8((color1 >> 5) & 0x3Fu);
    const uint8_t b1 = Expand5To8(color1 & 0x1Fu);

    uint32_t colorsRGB[4];
    colorsRGB[0] = PackRGBA(r0, g0, b0, 0);
    colorsRGB[1] = PackRGBA(r1, g1, b1, 0);
    colorsRGB[2] = PackRGBA(static_cast<uint8_t>((2u * r0 + r1) / 3u),
                            static_cast<uint8_t>((2u * g0 + g1) / 3u),
                            static_cast<uint8_t>((2u * b0 + b1) / 3u), 0);
    colorsRGB[3] = PackRGBA(static_cast<uint8_t>((r0 + 2u * r1) / 3u),
                            static_cast<uint8_t>((g0 + 2u * g1) / 3u),
                            static_cast<uint8_t>((b0 + 2u * b1) / 3u), 0);

    for (uint32_t y = 0; y < 4; ++y)
    {
        uint32_t* row = dest + y * stride;
        for (uint32_t x = 0; x < 4; ++x)
        {
            row[x] = colorsRGB[colorCode & 0x03u] | (uint32_t(alphas[alphaIndices & 0x07u]) << 24);
            colorCode >>= 2;
            alphaIndices >>= 3;
        }
    }
}

bool DDSParser::DecodeFile(const std::filesystem::path& filePath, ImageBuffer& outImage)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG_WARNING(Channels::graphics, "Failed to open DDS file: {}", filePath.string());
        return false;
    }

    const auto endPos = file.tellg();
    if (endPos <= 0) return false;
    const size_t fileSize = static_cast<size_t>(endPos);
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize)))
    {
        return false;
    }

    return DecodeMemory(buffer.data(), buffer.size(), outImage);
}

bool DDSParser::DecodeMemory(const uint8_t* data, size_t size, ImageBuffer& outImage)
{
    if (!data || size < 128)
    {
        return false;
    }

    uint32_t magic = 0;
    std::memcpy(&magic, data, sizeof(uint32_t));
    if (magic != 0x20534444u) // 'DDS '
    {
        return false;
    }

    DDS_HEADER header{};
    std::memcpy(&header, data + 4, sizeof(DDS_HEADER));
    const uint32_t width = header.dwWidth;
    const uint32_t height = header.dwHeight;

    if (width == 0 || height == 0 || width > 16384 || height > 16384)
    {
        return false;
    }

    const uint8_t* srcData = data + 128;
    const size_t srcSize = size - 128;
    const uint32_t fourCC = header.ddspf.dwFourCC;
    const uint32_t blockCountX = (width + 3) / 4;
    const uint32_t blockCountY = (height + 3) / 4;

    // Block-compressed formats: interior 4x4 blocks decode directly into destPixels;
    // partial boundary blocks decode via a 4x4 scratch tile with clipping.
    auto decodeBlocks = [&](size_t blockBytes, auto decodeBlock) -> bool {
        if (srcSize < static_cast<size_t>(blockCountX) * blockCountY * blockBytes)
        {
            LOG_WARNING(Channels::graphics, "DDS file is truncated");
            return false;
        }
        outImage.width = width;
        outImage.height = height;
        outImage.rgbaData.resize(static_cast<size_t>(width) * height * 4);
        auto* destPixels = reinterpret_cast<uint32_t*>(outImage.rgbaData.data());
        uint32_t tile[16];
        for (uint32_t by = 0; by < blockCountY; ++by)
        {
            const uint32_t py = by * 4;
            const uint32_t h = std::min(4u, height - py);
            for (uint32_t bx = 0; bx < blockCountX; ++bx)
            {
                const uint32_t px = bx * 4;
                const uint32_t w = std::min(4u, width - px);
                if (w == 4 && h == 4)
                {
                    decodeBlock(srcData, destPixels + static_cast<size_t>(py) * width + px, width);
                }
                else
                {
                    decodeBlock(srcData, tile, 4u);
                    for (uint32_t y = 0; y < h; ++y)
                    {
                        std::memcpy(destPixels + static_cast<size_t>(py + y) * width + px, tile + y * 4, w * 4);
                    }
                }
                srcData += blockBytes;
            }
        }
        return true;
    };

    if (fourCC == FOURCC_DXT1)
    {
        return decodeBlocks(8, [](const uint8_t* b, uint32_t* dst, uint32_t stride) {
            DecompressDXT1Block(b, dst, stride);
        });
    }
    if (fourCC == FOURCC_DXT5)
    {
        return decodeBlocks(16, [](const uint8_t* b, uint32_t* dst, uint32_t stride) {
            DecompressDXT5Block(b, dst, stride);
        });
    }
    if (fourCC == FOURCC_DXT3)
    {
        // DXT3 = 64 bits of explicit 4-bit alpha followed by a DXT1 colour block.
        return decodeBlocks(16, [](const uint8_t* b, uint32_t* dst, uint32_t stride) {
            DecompressDXT1Block(b + 8, dst, stride);
            uint64_t alphaBits = 0;
            std::memcpy(&alphaBits, b, sizeof(uint64_t));
            for (uint32_t y = 0; y < 4; ++y)
            {
                uint32_t* row = dst + y * stride;
                for (uint32_t x = 0; x < 4; ++x)
                {
                    const uint32_t a4 = static_cast<uint32_t>(alphaBits & 0x0Fu);
                    row[x] = (row[x] & 0x00FFFFFFu) | ((a4 * 17u) << 24);
                    alphaBits >>= 4;
                }
            }
        });
    }

    // Uncompressed RGB(A) with arbitrary channel masks (A8R8G8B8, X8R8G8B8, R8G8B8, A4R4G4B4, R5G6B5, ...).
    const uint32_t bpp = header.ddspf.dwRGBBitCount;
    if (fourCC == 0 && (bpp == 16 || bpp == 24 || bpp == 32))
    {
        const size_t bytesPerPixel = bpp / 8;
        const size_t totalPixels = static_cast<size_t>(width) * height;
        if (srcSize < totalPixels * bytesPerPixel)
        {
            LOG_WARNING(Channels::graphics, "DDS file is truncated");
            return false;
        }
        constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;
        const auto& pf = header.ddspf;
        const uint32_t aMask = (pf.dwFlags & DDPF_ALPHAPIXELS) ? pf.dwABitMask : 0;

        outImage.width = width;
        outImage.height = height;
        outImage.rgbaData.resize(totalPixels * 4);
        uint8_t* dst = outImage.rgbaData.data();

        // Fast path for standard 32-bit B8G8R8A8 / B8G8R8X8
        if (bpp == 32 && pf.dwRBitMask == 0x00FF0000u && pf.dwGBitMask == 0x0000FF00u && pf.dwBBitMask == 0x000000FFu)
        {
            const bool hasAlpha = (aMask == 0xFF000000u);
            for (size_t i = 0; i < totalPixels; ++i)
            {
                const uint8_t* s = srcData + i * 4;
                uint8_t* d = dst + i * 4;
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = hasAlpha ? s[3] : 255u;
            }
            return true;
        }

        struct ChannelSpec
        {
            uint32_t mask = 0;
            uint32_t shift = 0;
            uint32_t maxVal = 1;
            uint8_t fallback = 0;

            uint8_t Extract(uint32_t pixel) const noexcept
            {
                if (mask == 0) return fallback;
                return static_cast<uint8_t>(((pixel & mask) >> shift) * 255u / maxVal);
            }
        };

        auto makeSpec = [](uint32_t mask, uint8_t fallback) -> ChannelSpec {
            if (mask == 0) return { 0, 0, 1, fallback };
            const uint32_t shift = static_cast<uint32_t>(std::countr_zero(mask));
            return { mask, shift, mask >> shift, fallback };
        };

        const ChannelSpec rSpec = makeSpec(pf.dwRBitMask, 0);
        const ChannelSpec gSpec = makeSpec(pf.dwGBitMask, 0);
        const ChannelSpec bSpec = makeSpec(pf.dwBBitMask, 0);
        const ChannelSpec aSpec = makeSpec(aMask, 255);

        for (size_t i = 0; i < totalPixels; ++i)
        {
            uint32_t pixel = 0;
            std::memcpy(&pixel, srcData + i * bytesPerPixel, bytesPerPixel);
            uint8_t* d = dst + i * 4;
            d[0] = rSpec.Extract(pixel);
            d[1] = gSpec.Extract(pixel);
            d[2] = bSpec.Extract(pixel);
            d[3] = aSpec.Extract(pixel);
        }
        return true;
    }

    LOG_WARNING(Channels::graphics, "Unsupported DDS format FourCC: 0x{:08X}", fourCC);
    return false;
}

bool DDSParser::CropIconTile(const ImageBuffer& atlas, int tileIndex, int tileWidth, int tileHeight, ImageBuffer& outTile)
{
    if (tileIndex < 0 || atlas.width == 0 || atlas.height == 0 || tileWidth <= 0 || tileHeight <= 0)
    {
        return false;
    }
    if (atlas.rgbaData.size() < static_cast<size_t>(atlas.width) * atlas.height * 4)
    {
        return false;
    }

    const int cols = static_cast<int>(atlas.width) / tileWidth;
    const int rows = static_cast<int>(atlas.height) / tileHeight;
    if (cols <= 0 || rows <= 0)
    {
        return false;
    }

    const int tileX = (tileIndex % cols) * tileWidth;
    const int tileY = (tileIndex / cols) * tileHeight;
    if (tileX + tileWidth > static_cast<int>(atlas.width) || tileY + tileHeight > static_cast<int>(atlas.height))
    {
        return false;
    }

    outTile.width = static_cast<uint32_t>(tileWidth);
    outTile.height = static_cast<uint32_t>(tileHeight);
    outTile.rgbaData.resize(static_cast<size_t>(tileWidth) * tileHeight * 4);

    const size_t rowBytes = static_cast<size_t>(tileWidth) * 4;
    for (int y = 0; y < tileHeight; ++y)
    {
        const size_t srcOffset = (static_cast<size_t>(tileY + y) * atlas.width + static_cast<size_t>(tileX)) * 4;
        std::memcpy(&outTile.rgbaData[static_cast<size_t>(y) * rowBytes], &atlas.rgbaData[srcOffset], rowBytes);
    }

    return true;
}
} // namespace DNItemMaker::Graphics
