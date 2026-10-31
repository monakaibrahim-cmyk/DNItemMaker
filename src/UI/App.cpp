#include "UI/App.h"
#include "UI/MainFrame.h"
#include "UI/SplashWnd.h"

#include "Common/Crash/CrashHandler.h"
#include "Common/Logs/Logs.h"
#include "Data/ItemManager.h"
#include "version.h"

#include <filesystem>
#include <string_view>

// Single global MFC CWinApp object discovered by AfxWinMain (/ENTRY:wWinMainCRTStartup).
DNItemMaker::UI::CDNItemMakerApp theApp;

namespace DNItemMaker::UI {

namespace {

void EnablePerMonitorDpiAwareness() {
    if (HMODULE user32 = ::GetModuleHandleW(L"user32.dll")) {
        using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
        if (auto fn = reinterpret_cast<SetDpiCtxFn>(::GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == ((DPI_AWARENESS_CONTEXT)-4)
            fn(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)));
        }
    }
}

} // namespace

BOOL CDNItemMakerApp::InitInstance() {
    EnablePerMonitorDpiAwareness();

    // Deterministic single initialization of Common::Logs and Common::Crash.
    Common::Logs::Initialize(__LOG_DIRECTORY__, __LOG_FILE__);
    Common::Crash::Initialize(__LOG_DIRECTORY__);

    LOG_INFO(Channels::system, "Initializing {} v{} (Build: {})",
             Resources::Version::AppName,
             Resources::Version::VersionString,
             Resources::Version::BuildTimeStamp);

    // Optional headless verification mode: DNItemMaker.exe --verify-headless <resource_dir>
    int argc = 0;
    if (LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; ++i) {
            if (std::wstring_view(argv[i]) == L"--verify-headless") {
                const std::filesystem::path resDir = (i + 1 < argc)
                                                         ? std::filesystem::path(argv[i + 1])
                                                         : std::filesystem::path(R"(D:\GameRes\resource)");
                ::LocalFree(argv);

                Data::ItemManager mgr;
                if (!mgr.LoadWorkspace(resDir)) {
                    LOG_ERROR(Channels::engine, "Headless verification failed to load workspace: {}", resDir.string());
                    m_overrideExitCode = 1;
                    return FALSE;
                }
                std::string report;
                const bool ok = mgr.VerifyRoundTrip(report);
                LOG_INFO(Channels::engine, "Headless round-trip result: {}", ok ? "PASSED" : "FAILED");
                if (!ok) {
                    LOG_ERROR(Channels::engine, "Headless round-trip details:\n{}", report);
                }
                m_overrideExitCode = ok ? 0 : 1;
                Common::Logs::Instance().Flush();
                return FALSE; // Exit cleanly after headless verification without entering GUI message loop.
            }
        }
        ::LocalFree(argv);
    }

    // Enforce single instance: only 1 instance of the app can be run.
    m_singleInstanceMutex = ::CreateMutexW(nullptr, TRUE, L"Local\\DNItemMaker_SingleInstance_Mutex_9F8C");
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_WARNING(Channels::system, "Another instance of DNItemMaker is already running. Activating existing instance.");
        HWND existingWnd = nullptr;
        ::EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            wchar_t title[128]{};
            ::GetWindowTextW(hwnd, title, 128);
            if (wcsstr(title, L"Dragon Nest Item Maker")) {
                *reinterpret_cast<HWND*>(lParam) = hwnd;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&existingWnd));

        if (existingWnd) {
            if (::IsIconic(existingWnd)) {
                ::ShowWindow(existingWnd, SW_RESTORE);
            }
            ::SetForegroundWindow(existingWnd);
        }
        if (m_singleInstanceMutex) {
            ::CloseHandle(m_singleInstanceMutex);
            m_singleInstanceMutex = nullptr;
        }
        return FALSE;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    if (SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        m_comInitialized = true;
    }

    CSplashWnd splash;
    splash.CreateAndShow();
    splash.SetProgress(0.04f, "Initializing Studio Engine...", "Configuring runtime subsystems and memory stores");

    splash.SetProgress(0.08f, "Creating Studio Frame...", "Registering native MFC window surfaces");
    auto* frame = new CMainFrame();
    if (!frame->CreateMainWindow()) {
        LOG_FATAL(Channels::system, "Failed to create native MFC main frame window.");
        splash.CloseSplash();
        delete frame;
        return FALSE;
    }

    splash.SetProgress(0.12f, "Scanning Workspace...", "Detecting Dragon Nest resource directory");
    const auto initialRes = frame->GetEngine().DetectInitialWorkspacePath();

    if (!initialRes.empty()) {
        frame->LoadInitialWorkspaceWithSplash(initialRes, &splash);
    } else {
        splash.SetProgress(1.0f, "Ready", "No workspace directory configured. Open a resource folder to begin.");
        ::Sleep(200);
    }

    splash.CloseSplash();

    m_pMainWnd = frame;
    frame->ShowWindow(m_nCmdShow);
    frame->UpdateWindow();

    return TRUE;
}

int CDNItemMakerApp::ExitInstance() {
    LOG_INFO(Channels::system, "Shutting down {} cleanly.", Resources::Version::ShortName);
    Common::Logs::Instance().Flush();
    Common::Logs::Shutdown();

    if (m_singleInstanceMutex) {
        ::ReleaseMutex(m_singleInstanceMutex);
        ::CloseHandle(m_singleInstanceMutex);
        m_singleInstanceMutex = nullptr;
    }

    if (m_comInitialized) {
        ::CoUninitialize();
        m_comInitialized = false;
    }
    const int baseExit = CWinApp::ExitInstance();
    return (m_overrideExitCode >= 0) ? m_overrideExitCode : baseExit;
}

} // namespace DNItemMaker::UI
