cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(DNItemMaker LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "DNItemMaker is a native Windows MFC (atlmfc) application and requires MSVC on Windows.")
endif()

# Use MFC in a Shared DLL (_AFXDLL) and enforce C++23 across targets
set(CMAKE_MFC_FLAG 2)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# ---------------------------------------------------------------------------
# 0. Automatic Git Submodules Checkout
# ---------------------------------------------------------------------------
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SQLiteCpp/CMakeLists.txt" OR
       NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/zlib/CMakeLists.txt")
        message(STATUS "Git submodules are missing. Initializing automatically...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE GIT_SUBMOD_RESULT
        )
        if(NOT GIT_SUBMOD_RESULT EQUAL 0)
            message(WARNING "Failed to automatically initialize submodules via git.")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# System / Pre-installed Dependencies
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Submodules in extern/
# ---------------------------------------------------------------------------

# 1. zlib
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/zlib/CMakeLists.txt")
    set(ZLIB_BUILD_TESTING OFF CACHE INTERNAL "")
    set(ZLIB_BUILD_SHARED OFF CACHE INTERNAL "")
    set(ZLIB_BUILD_STATIC ON CACHE INTERNAL "")
    set(ZLIB_INSTALL OFF CACHE INTERNAL "")
    add_subdirectory(extern/zlib EXCLUDE_FROM_ALL)

    if(NOT TARGET ZLIB::ZLIB AND TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
else()
    find_package(ZLIB QUIET)
endif()

# 2. SQLiteCpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SQLiteCpp/CMakeLists.txt")
    set(SQLITECPP_INTERNAL_SQLITE ON CACHE INTERNAL "")
    set(SQLITECPP_BUILD_TESTS OFF CACHE INTERNAL "")
    set(SQLITECPP_BUILD_EXAMPLES OFF CACHE INTERNAL "")
    set(SQLITECPP_RUN_DOXYGEN OFF CACHE INTERNAL "")
    add_subdirectory(extern/SQLiteCpp EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# Project Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

set(RESOURCES "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.rc")
    set(RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.rc")
endif()

# Build as native Windows MFC GUI application (WIN32 subsystem)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

# C++23 with Unicode MFC
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23 c_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    _AFXDLL
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /MP)
target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:wWinMainCRTStartup)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

# ---------------------------------------------------------------------------
# Link Libraries
# ---------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ws2_32
    crypt32
    dbghelp
    psapi
    comctl32
    uxtheme
    shlwapi
)

if(TARGET SQLiteCpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE SQLiteCpp)
endif()

if(TARGET ZLIB::ZLIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
endif()

# ---------------------------------------------------------------------------
# Debug Symbols & Optimization Options
# ---------------------------------------------------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/O2 /Ob3 /Oi /Ot /GL /Gw /Gy /Zc:inline /Zi>
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF>
)
