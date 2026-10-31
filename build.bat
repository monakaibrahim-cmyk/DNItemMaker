@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: DNItemMaker - Build Script
:: ============================================================================

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

set "CONFIG=Release"
set "DO_RUN=0"

:: Parse arguments
:parse_args
if "%~1"=="" goto end_parse_args

if /i "%~1"=="debug" (
    set "CONFIG=Debug"
) else if /i "%~1"=="--debug" (
    set "CONFIG=Debug"
) else if /i "%~1"=="-d" (
    set "CONFIG=Debug"
) else if /i "%~1"=="release" (
    set "CONFIG=Release"
) else if /i "%~1"=="--release" (
    set "CONFIG=Release"
) else if /i "%~1"=="-r" (
    set "CONFIG=Release"
) else if /i "%~1"=="run" (
    set "DO_RUN=1"
) else if /i "%~1"=="--run" (
    set "DO_RUN=1"
) else if /i "%~1"=="help" (
    goto show_help
) else if /i "%~1"=="--help" (
    goto show_help
) else if /i "%~1"=="-h" (
    goto show_help
) else (
    echo [WARNING] Unknown option: %~1
)
shift
goto parse_args

:end_parse_args

:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake was not found in your PATH!
    echo Please install CMake or add it to your system PATH.
    goto exit_failure
)

echo ============================================================================
echo   DNItemMaker - Build System
echo   Configuration: %CONFIG%
echo ============================================================================

:: 1. Configure
echo [1/2] Configuring project with CMake...
cmake -B build
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    goto exit_failure
)

:: 2. Build
echo [2/2] Compiling project [%CONFIG%]...
cmake --build build --config %CONFIG% --parallel
if %errorlevel% neq 0 (
    echo [ERROR] Build compilation failed!
    goto exit_failure
)

echo ============================================================================
echo   [SUCCESS] Build completed successfully!
echo   Output: build\%CONFIG%\DNItemMaker.exe
echo ============================================================================

:: Launch if requested
if "%DO_RUN%"=="1" (
    echo.
    echo Launching DNItemMaker.exe...
    echo ----------------------------------------------------------------------------
    if exist "build\%CONFIG%\DNItemMaker.exe" (
        "build\%CONFIG%\DNItemMaker.exe"
    ) else (
        echo [ERROR] Executable not found at build\%CONFIG%\DNItemMaker.exe
    )
)

goto exit_success

:show_help
echo Usage: build.bat [options]
echo.
echo Options:
echo   release, --release, -r   Build in Release configuration (default)
echo   debug, --debug, -d       Build in Debug configuration
echo   run, --run               Run the application after building
echo   help, --help, -h         Display this help message
echo.
echo Examples:
echo   build.bat                Build in Release mode
echo   build.bat debug          Build in Debug mode
echo   build.bat run            Build and run Release
echo   build.bat debug run      Build and run Debug
echo.
goto exit_success

:exit_failure
echo [FAILURE] Build script terminated with errors.
call :check_pause
exit /b 1

:exit_success
call :check_pause
exit /b 0

:check_pause
if /i "%~1"=="--pause" pause
if /i "%~2"=="--pause" pause
exit /b 0
