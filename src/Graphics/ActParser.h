#pragma once

/**
 * @file ActParser.h
 * @brief Binary and manifest parser for Eternity Engine action files (.act).
 *
 * Implements parsing for Dragon Nest action scripts (versions 1..6) and manifest
 * lists, mapping high-level gameplay actions (Stand, Attack1, Move_Front, Jump, etc.)
 * to their underlying skeletal animation clips and frame bounds.
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DNItemMaker::Graphics {

/**
 * @brief Individual action definition linking a gameplay action to an animation clip.
 */
struct ActionEntry {
    std::string name;             ///< Action name (e.g. "Stand", "Attack1", "Move_Front").
    std::string linkAniName;      ///< Underlying .ani clip name (e.g. "CH01_Warrior_10001_Stand_Fighting").
    uint32_t length = 0;          ///< Action duration in frames.
    std::string nextActionName;   ///< Following action or loop target (e.g. "Stand").
    uint32_t blendFrame = 0;      ///< Blend frame transition length.
    uint32_t nextActionFrame = 0; ///< Frame threshold to trigger next action.
};

/**
 * @brief Parsed action file dataset containing discrete gameplay actions.
 */
struct ParsedActionFile {
    bool valid = false;
    std::string errorMessage;
    std::filesystem::path filePath;
    int32_t version = 0;
    std::vector<ActionEntry> actions;

    /** @brief Locates action by name (-1 if not found, case-insensitive). */
    int32_t FindActionIndex(const std::string& actionName) const noexcept;

    /** @brief Returns pointer to action by name (nullptr if not found). */
    const ActionEntry* FindAction(const std::string& actionName) const noexcept;
};

/**
 * @class ActParser
 * @brief Static loader for Eternity Engine .act files (binary and manifest).
 */
class ActParser {
public:
    /** @brief Loads and parses a .act file from filesystem path. */
    static ParsedActionFile LoadFromFile(const std::filesystem::path& path);

    /** @brief Parses raw in-memory .act byte buffer. */
    static ParsedActionFile LoadFromMemory(const uint8_t* data, size_t size,
                                           const std::filesystem::path& basePath = {},
                                           const std::string& debugPath = {});
};

} // namespace DNItemMaker::Graphics
