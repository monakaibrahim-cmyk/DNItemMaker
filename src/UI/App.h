#pragma once

/**
 * @file App.h
 * @brief Application entry point and MFC CWinApp lifecycle controller.
 *
 * Manages application initialization, single-instance mutex enforcement (bringing
 * existing window to foreground on duplicate launch), COM apartment initialization,
 * and command-line headless verification dispatch (--verify-headless).
 */

#include "UI/MfcCommon.h"

namespace DNItemMaker::UI {

/**
 * @class CDNItemMakerApp
 * @brief Main application object deriving from MFC CWinApp.
 */
class CDNItemMakerApp : public CWinApp {
public:
    CDNItemMakerApp() = default;

    /**
     * @brief Application initialization entry point.
     * Enforces single-instance mutex, parses command line flags, creates main window.
     */
    BOOL InitInstance() override;

    /**
     * @brief Application shutdown cleanup. Releases single-instance mutex and uninitializes COM.
     */
    int ExitInstance() override;

private:
    HANDLE m_singleInstanceMutex = nullptr;
    bool m_comInitialized = false;
    int m_overrideExitCode = -1;
};

} // namespace DNItemMaker::UI
