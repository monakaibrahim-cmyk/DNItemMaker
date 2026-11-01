#include "Graphics/ActConverter.h"
#include <Common/Logs/Logs.h>

#include <cstring>
#include <fstream>
#include <format>

namespace DNItemMaker::Graphics {

namespace {
constexpr const char* kActMagic = "Eternity Action File";
constexpr size_t kHeaderMagicSize = 32;

struct CursorReader {
    const uint8_t* p = nullptr;
    const uint8_t* end = nullptr;
    bool ok = true;

    CursorReader(const uint8_t* data, size_t size) : p(data), end(data + size) {}

    bool CanRead(size_t n) const noexcept {
        return ok && (p + n <= end);
    }

    template <typename T>
    T Read() noexcept {
        T val{};
        if (!CanRead(sizeof(T))) {
            ok = false;
            return val;
        }
        std::memcpy(&val, p, sizeof(T));
        p += sizeof(T);
        return val;
    }

    void Skip(size_t n) noexcept {
        if (!CanRead(n)) {
            ok = false;
            return;
        }
        p += n;
    }

    const uint8_t* ReadBytes(size_t n) noexcept {
        if (!CanRead(n)) {
            ok = false;
            return nullptr;
        }
        const uint8_t* ptr = p;
        p += n;
        return ptr;
    }
};

void WriteInt32(std::vector<uint8_t>& buf, int32_t val) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), ptr, ptr + 4);
}

void WriteBytes(std::vector<uint8_t>& buf, const uint8_t* src, size_t count) {
    if (src && count > 0) {
        buf.insert(buf.end(), src, src + count);
    }
}

bool CopyCString(CursorReader& reader, std::vector<uint8_t>& dest) {
    const int32_t len = reader.Read<int32_t>();
    if (!reader.ok || len < 0) return false;
    WriteInt32(dest, len);
    if (len > 0) {
        const uint8_t* strBytes = reader.ReadBytes(static_cast<size_t>(len));
        if (!strBytes) return false;
        WriteBytes(dest, strBytes, static_cast<size_t>(len));
    }
    return true;
}
} // namespace

bool ActConverter::ProbeActFile(const std::filesystem::path& path,
                                int32_t& outVersion,
                                int32_t& outCount,
                                bool& outIsManifest) {
    outVersion = 0;
    outCount = 0;
    outIsManifest = false;

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint8_t hdr[40] = {0};
    f.read(reinterpret_cast<char*>(hdr), sizeof(hdr));
    const auto readBytes = f.gcount();
    if (readBytes < 32) return false;

    if (std::memcmp(hdr, kActMagic, std::strlen(kActMagic)) == 0) {
        if (readBytes < 40) return false;
        std::memcpy(&outVersion, hdr + 32, 4);
        std::memcpy(&outCount, hdr + 36, 4);
        outIsManifest = false;
        return true;
    }

    // Check if manifest text
    std::string text(reinterpret_cast<const char*>(hdr), static_cast<size_t>(readBytes));
    if (text.find(".act") != std::string::npos) {
        outIsManifest = true;
        return true;
    }

    return false;
}

bool ActConverter::ConvertMemoryV6ToV5(const uint8_t* inData, size_t inSize,
                                      std::vector<uint8_t>& outV5Data,
                                      int32_t& outActionCount,
                                      std::string& outError) {
    outActionCount = 0;
    outError.clear();

    if (!inData || inSize < 40) {
        outError = "Buffer too small for ACT header (minimum 40 bytes)";
        return false;
    }

    if (std::memcmp(inData, kActMagic, std::strlen(kActMagic)) != 0) {
        outError = "Not a valid binary Eternity Action File (magic header mismatch)";
        return false;
    }

    int32_t version = 0;
    int32_t actionCount = 0;
    std::memcpy(&version, inData + 32, 4);
    std::memcpy(&actionCount, inData + 36, 4);

    if (version != 6) {
        outError = std::format("Input file is version {} (expected version 6)", version);
        return false;
    }

    if (actionCount < 0 || actionCount > 65536) {
        outError = std::format("Invalid action count in header: {}", actionCount);
        return false;
    }

    outV5Data.clear();
    outV5Data.reserve(inSize); // v5 is slightly smaller than v6

    // 1. Write Header: 32 bytes magic + int32 version (5) + int32 actionCount
    WriteBytes(outV5Data, inData, kHeaderMagicSize);
    WriteInt32(outV5Data, 5); // Version 5
    WriteInt32(outV5Data, actionCount);

    CursorReader cur(inData + 40, inSize - 40);

    for (int32_t a = 0; a < actionCount; ++a) {
        // szName
        if (!CopyCString(cur, outV5Data)) {
            outError = std::format("Failed to read action name at index {}", a);
            return false;
        }
        // szLinkAniName
        if (!CopyCString(cur, outV5Data)) {
            outError = std::format("Failed to read link animation name at index {}", a);
            return false;
        }
        // dwLength
        const uint32_t dwLength = cur.Read<uint32_t>();
        WriteBytes(outV5Data, reinterpret_cast<const uint8_t*>(&dwLength), 4);

        // szNextActionName
        if (!CopyCString(cur, outV5Data)) {
            outError = std::format("Failed to read next action name at index {}", a);
            return false;
        }

        // dwBlendFrame (4) + dwNextActionFrame (4)
        const uint8_t* blendNext = cur.ReadBytes(8);
        if (!blendNext) {
            outError = std::format("Truncated blend/next frame data at action {}", a);
            return false;
        }
        WriteBytes(outV5Data, blendNext, 8);

        // dwUnkAct2 (4) + dwUnkAct3 (4) + bUnkAct4 (1) + dwUnkAct5 (4)
        const uint8_t* actFields = cur.ReadBytes(13);
        if (!actFields) {
            outError = std::format("Truncated version flags at action {}", a);
            return false;
        }
        WriteBytes(outV5Data, actFields, 13);

        // bUnkAct6 (1 byte) - EXCLUSIVE TO VERSION 6! Strip this byte!
        const uint8_t bUnkAct6 = cur.Read<uint8_t>();
        if (!cur.ok) {
            outError = std::format("Failed to read version 6 bUnkAct6 byte at action {}", a);
            return false;
        }

        // Signals count
        const int32_t sigCount = cur.Read<int32_t>();
        if (!cur.ok || sigCount < 0 || sigCount > 65536) {
            outError = std::format("Invalid signal count {} at action {}", sigCount, a);
            return false;
        }
        WriteInt32(outV5Data, sigCount);

        for (int32_t s = 0; s < sigCount; ++s) {
            // Header block: sigIndex (4), startFrame (4), endFrame (4), yOrder (4) = 16 bytes
            const uint8_t* sigHdr = cur.ReadBytes(16);
            if (!sigHdr) {
                outError = std::format("Truncated signal header at action {} signal {}", a, s);
                return false;
            }
            WriteBytes(outV5Data, sigHdr, 16);

            // Signal buffer (64 ints = 256 bytes)
            const uint8_t* sigBuf = cur.ReadBytes(256);
            if (!sigBuf) {
                outError = std::format("Truncated signal buffer at action {} signal {}", a, s);
                return false;
            }
            WriteBytes(outV5Data, sigBuf, 256);

            // Vector2 list
            const int32_t v2Count = cur.Read<int32_t>();
            if (!cur.ok || v2Count < 0) return false;
            WriteInt32(outV5Data, v2Count);
            if (v2Count > 0) {
                const size_t bytes = static_cast<size_t>(v2Count) * 12; // 4 table + 8 EtVector2
                const uint8_t* v2Data = cur.ReadBytes(bytes);
                if (!v2Data) return false;
                WriteBytes(outV5Data, v2Data, bytes);
            }

            // Vector3 list
            const int32_t v3Count = cur.Read<int32_t>();
            if (!cur.ok || v3Count < 0) return false;
            WriteInt32(outV5Data, v3Count);
            if (v3Count > 0) {
                const size_t bytes = static_cast<size_t>(v3Count) * 16; // 4 table + 12 EtVector3
                const uint8_t* v3Data = cur.ReadBytes(bytes);
                if (!v3Data) return false;
                WriteBytes(outV5Data, v3Data, bytes);
            }

            // Vector4 list
            const int32_t v4Count = cur.Read<int32_t>();
            if (!cur.ok || v4Count < 0) return false;
            WriteInt32(outV5Data, v4Count);
            if (v4Count > 0) {
                const size_t bytes = static_cast<size_t>(v4Count) * 20; // 4 table + 16 EtVector4
                const uint8_t* v4Data = cur.ReadBytes(bytes);
                if (!v4Data) return false;
                WriteBytes(outV5Data, v4Data, bytes);
            }

            // String list
            const int32_t strCount = cur.Read<int32_t>();
            if (!cur.ok || strCount < 0) return false;
            WriteInt32(outV5Data, strCount);
            if (strCount > 0) {
                const size_t tableBytes = static_cast<size_t>(strCount) * 4;
                const uint8_t* tbl = cur.ReadBytes(tableBytes);
                if (!tbl) return false;
                WriteBytes(outV5Data, tbl, tableBytes);

                for (int32_t st = 0; st < strCount; ++st) {
                    if (!CopyCString(cur, outV5Data)) {
                        outError = std::format("Failed to read signal string {} at action {} signal {}", st, a, s);
                        return false;
                    }
                }
            }

            // In version 6, if bUnkAct6 is non-zero, there is a 1-byte trailer at the end of each signal.
            // OMIT THIS 1-BYTE TRAILER IN VERSION 5!
            if (bUnkAct6 != 0) {
                cur.Skip(1);
            }
        }
    }

    if (!cur.ok) {
        outError = "Premature end of file while decoding actions";
        return false;
    }

    outActionCount = actionCount;
    return true;
}

ActConvertResult ActConverter::ConvertFileV6ToV5(const std::filesystem::path& inputPath,
                                                const std::filesystem::path& outputPath,
                                                bool backupOriginal) {
    ActConvertResult res;
    res.filePath = inputPath;
    res.outputPath = outputPath;

    std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        res.message = "Failed to open input file";
        return res;
    }
    const auto size = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> inBuf(size);
    if (!in.read(reinterpret_cast<char*>(inBuf.data()), static_cast<std::streamsize>(size))) {
        res.message = "Failed to read input file bytes";
        return res;
    }
    in.close();

    res.bytesBefore = size;

    if (size < 40) {
        res.message = "File too small for binary ACT header";
        return res;
    }

    int32_t ver = 0;
    std::memcpy(&ver, inBuf.data() + 32, 4);
    res.versionBefore = ver;

    if (ver != 6) {
        if (ver == 5) {
            res.message = "File is already Version 5 (skipped)";
        } else {
            res.message = std::format("File is Version {} (not Version 6)", ver);
        }
        return res;
    }

    std::vector<uint8_t> outV5;
    int32_t count = 0;
    std::string err;
    if (!ConvertMemoryV6ToV5(inBuf.data(), inBuf.size(), outV5, count, err)) {
        res.message = std::format("Conversion failed: {}", err);
        return res;
    }

    res.actionCount = count;
    res.bytesAfter = outV5.size();
    res.versionAfter = 5;

    // Create backup if writing in-place
    if (backupOriginal && inputPath == outputPath) {
        std::error_code ec;
        auto bakPath = inputPath;
        bakPath += ".bak";
        std::filesystem::copy_file(inputPath, bakPath, std::filesystem::copy_options::overwrite_existing, ec);
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        res.message = "Failed to open output file for writing";
        return res;
    }
    if (!out.write(reinterpret_cast<const char*>(outV5.data()), static_cast<std::streamsize>(outV5.size()))) {
        res.message = "Failed to write output bytes to disk";
        return res;
    }
    out.close();

    res.success = true;
    res.message = std::format("Successfully converted {} actions (saved {} bytes)",
                              count, (res.bytesBefore > res.bytesAfter) ? (res.bytesBefore - res.bytesAfter) : 0);

    LOG_INFO(Channels::graphics, "Converted ACT {} v6 -> v5 ({} actions, {} -> {} bytes)",
             inputPath.string(), count, res.bytesBefore, res.bytesAfter);

    return res;
}

std::vector<ActConvertResult> ActConverter::ConvertDirectoryV6ToV5(const std::filesystem::path& directoryPath,
                                                                 bool recursive,
                                                                 bool backupOriginal) {
    std::vector<ActConvertResult> results;
    std::error_code ec;

    auto processPath = [&](const std::filesystem::path& p) {
        if (p.has_extension() && _stricmp(p.extension().string().c_str(), ".act") == 0) {
            int32_t ver = 0;
            int32_t cnt = 0;
            bool isManifest = false;
            if (ProbeActFile(p, ver, cnt, isManifest) && ver == 6) {
                results.push_back(ConvertFileV6ToV5(p, p, backupOriginal));
            }
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath, ec)) {
            if (entry.is_regular_file()) {
                processPath(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath, ec)) {
            if (entry.is_regular_file()) {
                processPath(entry.path());
            }
        }
    }

    return results;
}

} // namespace DNItemMaker::Graphics
