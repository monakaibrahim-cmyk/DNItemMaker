@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: DNItemMaker - Clean Script
:: ============================================================================

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

echo ============================================================================
echo   DNItemMaker - Clean Build Artifacts
echo ============================================================================

set "CLEANED=0"

:: 1. Remove build directory
if exist "build" (
    echo [*] Removing 'build' directory...
    rmdir /s /q "build"
    if exist "build" (
        echo [WARNING] Could not immediately delete 'build' [file in use?]. Retrying...
        timeout /t 1 /nobreak >nul
        rmdir /s /q "build"
    )
    if not exist "build" (
        echo [OK] 'build' directory removed.
        set "CLEANED=1"
    ) else (
        echo [ERROR] Failed to completely remove 'build' directory.
    )
) else (
    echo [*] 'build' directory does not exist. Nothing to clean.
)

:: 2. Optional: If 'all' is passed, also clean .vs and Logs
if /i "%~1"=="all" (
    if exist ".vs" (
        echo [*] Removing '.vs' Visual Studio cache...
        rmdir /s /q ".vs"
        echo [OK] '.vs' directory removed.
        set "CLEANED=1"
    )
    if exist "Logs" (
        echo [*] Removing 'Logs' directory...
        rmdir /s /q "Logs"
        echo [OK] 'Logs' directory removed.
        set "CLEANED=1"
    )
)

echo ============================================================================
if "%CLEANED%"=="1" (
    echo   [SUCCESS] Clean completed!
) else (
    echo   [INFO] Workspace is already clean.
)
echo ============================================================================

:: Optional pause if passed --pause or if double-clicked
if /i "%~1"=="--pause" pause
if /i "%~2"=="--pause" pause
exit /b 0
