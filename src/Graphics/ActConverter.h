#pragma once

/**
 * @file ActConverter.h
 * @brief High-performance converter from Eternity Engine ACT version 6 to version 5.
 *
 * Dragon Nest client builds and private servers up to 32-bit Eternity Engine v11
 * strictly require Action File version <= 5 (Header.nVersion <= 5). Modern 64-bit
 * resource packages introduced ACT version 6, which adds a 1-byte flag (bUnkAct6)
 * per action entry and an optional 1-byte trailer per signal.
 *
 * ActConverter reliably strips the version 6 extensions and adjusts the header
 * to produce fully compatible, crash-free version 5 action files.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Detailed outcome of an individual .act conversion operation.
 */
struct ActConvertResult {
    bool success = false;
    std::filesystem::path filePath;
    std::filesystem::path outputPath;
    int32_t versionBefore = 0;
    int32_t versionAfter = 0;
    int32_t actionCount = 0;
    size_t bytesBefore = 0;
    size_t bytesAfter = 0;
    std::string message;
};

/**
 * @class ActConverter
 * @brief Utility for converting Dragon Nest .act files from version 6 to version 5.
 */
class ActConverter {
public:
    /**
     * @brief Converts an in-memory buffer of a version 6 .act file to version 5.
     * @param inData Pointer to raw version 6 .act bytes.
     * @param inSize Size in bytes of input buffer.
     * @param outV5Data Destination buffer receiving converted version 5 bytes.
     * @param outActionCount Receives count of actions converted.
     * @param outError Error message if conversion fails.
     * @return true on success, false on invalid format or corruption.
     */
    static bool ConvertMemoryV6ToV5(const uint8_t* inData, size_t inSize,
                                   std::vector<uint8_t>& outV5Data,
                                   int32_t& outActionCount,
                                   std::string& outError);

    /**
     * @brief Converts a single .act file from version 6 to version 5.
     * @param inputPath Source .act file.
     * @param outputPath Destination .act file (can be same as inputPath).
     * @param backupOriginal If true and outputPath == inputPath, creates .bak before overwriting.
     * @return ActConvertResult with conversion statistics.
     */
    static ActConvertResult ConvertFileV6ToV5(const std::filesystem::path& inputPath,
                                             const std::filesystem::path& outputPath,
                                             bool backupOriginal = true);

    /**
     * @brief Scans a directory and converts all version 6 .act files to version 5.
     * @param directoryPath Directory to scan.
     * @param recursive Whether to scan subdirectories.
     * @param backupOriginal Whether to create .bak backups.
     * @return List of conversion results for every file processed.
     */
    static std::vector<ActConvertResult> ConvertDirectoryV6ToV5(const std::filesystem::path& directoryPath,
                                                               bool recursive = true,
                                                               bool backupOriginal = true);

    /**
     * @brief Inspects a .act file and returns its detected version and action count.
     * @param path Path to .act file.
     * @param outVersion Receives detected version (1..6) or 0 if text manifest / invalid.
     * @param outCount Receives action count.
     * @param outIsManifest Receives true if file is a multi-action text manifest.
     * @return true if valid .act file.
     */
    static bool ProbeActFile(const std::filesystem::path& path,
                             int32_t& outVersion,
                             int32_t& outCount,
                             bool& outIsManifest);
};

} // namespace DNItemMaker::Graphics
