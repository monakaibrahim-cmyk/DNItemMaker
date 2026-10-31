#pragma once

#include "UI/MfcCommon.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>

namespace DNItemMaker::UI {

inline constexpr UINT WM_APP_SPLASH_PROGRESS = WM_APP + 901;

/**
 * @class CSplashWnd
 * @brief High-fidelity Ghidra-style splash screen with real-time asset and table loading.
 *
 * Displays application branding, version identity, a double-buffered custom progress bar,
 * and live file/stage progress text during startup before the primary CMainFrame window appears.
 * Designed with full multithread safety so background loader threads never block or deadlock.
 */
class CSplashWnd : public CWnd {
public:
    CSplashWnd();
    ~CSplashWnd() override;

    /**
     * @brief Creates and centers the splash window on the primary display.
     * @return true on success, false otherwise.
     */
    bool CreateAndShow();

    /**
     * @brief Updates loading progress in a thread-safe manner without blocking background workers.
     * @param progress Normalized progress [0.0f .. 1.0f].
     * @param stage Primary subsystem or stage label.
     * @param detail Specific table or file currently being processed.
     */
    void SetProgress(float progress, std::string_view stage, std::string_view detail);

    /**
     * @brief Destroys and closes the splash window.
     */
    void CloseSplash();

    /**
     * @brief Pumps any pending UI messages on the main thread.
     */
    void PumpMessages();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnSplashProgress(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    mutable std::mutex m_mutex;
    DWORD m_mainThreadId = 0;

    float m_progress = 0.0f;
    std::string m_stageTitle = "Starting Studio Engine...";
    std::string m_detailText = "Initializing Dragon Nest core runtime";

    HICON m_hIcon = nullptr;
    CFont m_fontTitle;
    CFont m_fontSubtitle;
    CFont m_fontStage;
    CFont m_fontDetail;
    CFont m_fontMeta;
};

} // namespace DNItemMaker::UI
