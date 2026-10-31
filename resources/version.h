#pragma once

// Application Identity & Metadata
#define APP_NAME               "Dragon Nest Item Maker"
#define APP_NAME_SHORT         "DNItemMaker"
#define APP_EXE_NAME           "DNItemMaker.exe"
#define COMPANY_NAME           "OpenSource Studios"
#define LEGAL_COPYRIGHT        "Copyright (C) 2025-2026 OpenSource Studios. All rights reserved."
#define FILE_DESCRIPTION       "Dragon Nest Item Maker and Editor."
#define ORIGINAL_FILENAME      "DNItemMaker.exe"

#define BUILD_TIMESTAMP        __DATE__ " " __TIME__

// Semantic Version Numbers (Major.Minor.Patch.Build)
#define VERSION_MAJOR          1
#define VERSION_MINOR          0
#define VERSION_PATCH          0
#define VERSION_BUILD          0

#define VERSION_STRING         "1.0.0.0"
#define VERSION_SHORT_STRING   "1.0.0"

// Windows Resource Identifiers
#define IDI_APP_ICON                     101

#ifdef __cplusplus
#include <string_view>

namespace Resources::Version
{
    inline constexpr std::string_view AppName = APP_NAME;
    inline constexpr std::string_view ShortName = APP_NAME_SHORT;
    inline constexpr std::string_view ExeName = APP_EXE_NAME;
    inline constexpr std::string_view CompanyName = COMPANY_NAME;
    inline constexpr std::string_view Copyright = LEGAL_COPYRIGHT;
    inline constexpr std::string_view Description = FILE_DESCRIPTION;
    inline constexpr std::string_view VersionString = VERSION_STRING;
    inline constexpr std::string_view ShortVersionString = VERSION_SHORT_STRING;
    inline constexpr std::string_view BuildTimeStamp = BUILD_TIMESTAMP;
    inline constexpr int Major = VERSION_MAJOR;
    inline constexpr int Minor = VERSION_MINOR;
    inline constexpr int Patch = VERSION_PATCH;
    inline constexpr int Build = VERSION_BUILD;
    inline constexpr int AppIconId = IDI_APP_ICON;
}
#endif
