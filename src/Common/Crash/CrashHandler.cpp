#include <Common/Crash/CrashHandler.h>
#include <Common/Logs/Logs.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stacktrace>
#include <string>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <DbgHelp.h>
    #include <psapi.h>
    #include <crtdbg.h>
    #pragma comment(lib, "dbghelp.lib")
    #pragma comment(lib, "psapi.lib")
#else
    #include <sys/resource.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #if defined(__linux__)
        #include <ucontext.h>
    #endif
#endif

namespace Common::Crash
{
namespace
{
    std::string g_dumpDirectory = "Logs";
    std::mutex g_crashMutex;
    bool g_handlingCrash = false;

    // Helper: format timestamp
    std::string GetTimestampString()
    {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        return std::format("{:04d}-{:02d}-{:02d}_{:02d}-{:02d}-{:02d}",
                           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                           tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    uint32_t GetCurrentProcessIdVal()
    {
#if defined(_WIN32)
        return static_cast<uint32_t>(::GetCurrentProcessId());
#else
        return static_cast<uint32_t>(::getpid());
#endif
    }

    uint32_t GetCurrentThreadIdVal()
    {
#if defined(_WIN32)
        return static_cast<uint32_t>(::GetCurrentThreadId());
#else
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pthread_self()));
#endif
    }

    // Helper: Dump memory / system statistics
    std::string GetSystemMemoryInfo()
    {
        std::ostringstream ss;
#if defined(_WIN32)
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus))
        {
            ss << std::format("  Physical Memory : {:.2f} MB used / {:.2f} MB total ({}% load)\n",
                              (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0),
                              memStatus.ullTotalPhys / (1024.0 * 1024.0),
                              memStatus.dwMemoryLoad);
            ss << std::format("  Pagefile Memory : {:.2f} MB available / {:.2f} MB total\n",
                              memStatus.ullAvailPageFile / (1024.0 * 1024.0),
                              memStatus.ullTotalPageFile / (1024.0 * 1024.0));
        }

        PROCESS_MEMORY_COUNTERS_EX pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        {
            ss << std::format("  Process Working Set: {:.2f} MB (Peak: {:.2f} MB)\n",
                              pmc.WorkingSetSize / (1024.0 * 1024.0),
                              pmc.PeakWorkingSetSize / (1024.0 * 1024.0));
            ss << std::format("  Process Commit     : {:.2f} MB\n",
                              pmc.PrivateUsage / (1024.0 * 1024.0));
        }
#else
        struct rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            ss << std::format("  Process Max RSS: {:.2f} MB\n", usage.ru_maxrss / 1024.0);
        }
#endif
        return ss.str();
    }

#if defined(_WIN32)
    const char* ExceptionCodeToString(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION (Access Violation)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (Array Bounds Exceeded)";
        case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT (Breakpoint Encountered)";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT (Data Misalignment)";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO (Float Divide By Zero)";
        case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW (Float Overflow)";
        case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW (Float Underflow)";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION (Illegal Instruction)";
        case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR (In Page Error)";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO (Integer Divide By Zero)";
        case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW (Integer Overflow)";
        case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION (Invalid Disposition)";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION (Non-continuable Exception)";
        case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION (Privileged Instruction)";
        case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW (Stack Overflow)";
        case 0xE06D7363:                         return "MSVC C++ Exception";
        default:                                 return "UNKNOWN_EXCEPTION";
        }
    }

    std::string FormatRegisters(const CONTEXT* ctx)
    {
        if (!ctx) return "No register context available.\n";
        std::ostringstream ss;
#if defined(_M_AMD64)
        ss << std::format("  RIP: 0x{:016X}  RSP: 0x{:016X}  RBP: 0x{:016X}\n", ctx->Rip, ctx->Rsp, ctx->Rbp);
        ss << std::format("  RAX: 0x{:016X}  RBX: 0x{:016X}  RCX: 0x{:016X}  RDX: 0x{:016X}\n", ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx);
        ss << std::format("  RSI: 0x{:016X}  RDI: 0x{:016X}  R8:  0x{:016X}  R9:  0x{:016X}\n", ctx->Rsi, ctx->Rdi, ctx->R8, ctx->R9);
        ss << std::format("  R10: 0x{:016X}  R11: 0x{:016X}  R12: 0x{:016X}  R13: 0x{:016X}\n", ctx->R10, ctx->R11, ctx->R12, ctx->R13);
        ss << std::format("  R14: 0x{:016X}  R15: 0x{:016X}  EFLAGS: 0x{:08X}\n", ctx->R14, ctx->R15, ctx->EFlags);
#elif defined(_M_IX86)
        ss << std::format("  EIP: 0x{:08X}  ESP: 0x{:08X}  EBP: 0x{:08X}\n", ctx->Eip, ctx->Esp, ctx->Ebp);
        ss << std::format("  EAX: 0x{:08X}  EBX: 0x{:08X}  ECX: 0x{:08X}  EDX: 0x{:08X}\n", ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
        ss << std::format("  ESI: 0x{:08X}  EDI: 0x{:08X}  EFLAGS: 0x{:08X}\n", ctx->Esi, ctx->Edi, ctx->EFlags);
#elif defined(_M_ARM64)
        ss << std::format("  PC: 0x{:016X}  SP: 0x{:016X}  FP: 0x{:016X}  LR: 0x{:016X}\n", ctx->Pc, ctx->Sp, ctx->Fp, ctx->Lr);
#endif
        return ss.str();
    }

    bool WriteMiniDumpFile(const std::string& dumpPath, EXCEPTION_POINTERS* ep)
    {
        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
            MiniDumpNormal |
            MiniDumpWithDataSegs |
            MiniDumpWithIndirectlyReferencedMemory |
            MiniDumpWithProcessThreadData |
            MiniDumpWithThreadInfo |
            MiniDumpWithHandleData |
            MiniDumpWithUnloadedModules
        );

        BOOL success = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            dumpType,
            ep ? &mei : nullptr,
            nullptr,
            nullptr
        );

        CloseHandle(hFile);
        return success == TRUE;
    }
#endif

    const char* SignalToString(int signum)
    {
        switch (signum)
        {
        case SIGSEGV: return "SIGSEGV (Segmentation Fault - Invalid Memory Access)";
        case SIGABRT: return "SIGABRT (Abort Signal)";
        case SIGFPE:  return "SIGFPE (Floating Point / Arithmetic Exception)";
        case SIGILL:  return "SIGILL (Illegal Instruction)";
#if defined(SIGBUS)
        case SIGBUS:  return "SIGBUS (Bus Error - Bad Memory Alignment/Access)";
#endif
        case SIGTERM: return "SIGTERM (Termination Request)";
        case SIGINT:  return "SIGINT (Interrupt Signal)";
        default:      return "Unknown Signal";
        }
    }

    void GenerateCrashReport(Severity severity,
                             const std::string& reason,
                             const std::string& details,
                             const std::string& registerState,
                             const std::stacktrace& trace,
                             const std::string& minidumpPath)
    {
        std::error_code ec;
        std::filesystem::create_directories(g_dumpDirectory, ec);

        std::string timestamp = GetTimestampString();
        uint32_t pid = GetCurrentProcessIdVal();
        uint32_t tid = GetCurrentThreadIdVal();

        std::string crashFilename = std::format("Crash_{}_{}.log", timestamp, pid);
        std::string reportPath = std::format("{}/{}", g_dumpDirectory, crashFilename);

        try
        {
            Common::Logs crashLogger(g_dumpDirectory, crashFilename);

            crashLogger.WriteF(severity, Channels::system, "================================================================================");
            crashLogger.WriteF(severity, Channels::system, "APPLICATION CRASH INTERCEPTED (PID: {}, Thread: {})", pid, tid);
            crashLogger.WriteF(severity, Channels::system, "Reason: {}", reason);
            if (!details.empty())
            {
                crashLogger.WriteF(severity, Channels::system, "Details: {}", details);
            }
            if (!minidumpPath.empty())
            {
                crashLogger.WriteF(severity, Channels::system, "Minidump File: {}", minidumpPath);
            }
            crashLogger.WriteF(severity, Channels::system, "System Memory Metrics:\n{}", GetSystemMemoryInfo());
            if (!registerState.empty())
            {
                crashLogger.WriteF(severity, Channels::system, "CPU Registers:\n{}", registerState);
            }
            if (!trace.empty())
            {
                std::ostringstream stStr;
                stStr << "Call Stack Backtrace (" << trace.size() << " frames):\n";
                size_t frameIndex = 0;
                for (const auto& frame : trace)
                {
                    stStr << std::format("  #{:<2} {:<40} [{}:{}]\n",
                                         frameIndex++,
                                         frame.description(),
                                         frame.source_file(),
                                         frame.source_line());
                }
                crashLogger.WriteF(severity, Channels::system, "{}", stStr.str());
            }
            crashLogger.WriteF(severity, Channels::system, "Crash report recorded to: {}", reportPath);
            crashLogger.WriteF(severity, Channels::system, "================================================================================");
            crashLogger.Flush();

            // Also record a notice in the primary application log (DNItemMaker.log) using WriteF
            Common::Logs::Instance().WriteF(severity, Channels::system,
                "Crash report recorded via Common::Logs extension to: {} (Dump: {})",
                reportPath, minidumpPath.empty() ? "None" : minidumpPath);
            Common::Logs::Instance().Flush();
        }
        catch (...)
        {
        }

        // Output to standard error
        std::cerr << "\n"
                  << "********************************************************************************\n"
                  << " [" << (severity == Severity::fatal ? "FATAL" : "ERROR") << "] APPLICATION CRASH INTERCEPTED\n"
                  << " Reason: " << reason << "\n"
                  << " Crash Log  : " << reportPath << "\n";
        if (!minidumpPath.empty())
        {
            std::cerr << " Memory Dump: " << minidumpPath << "\n";
        }
        std::cerr << "********************************************************************************\n\n";

#if defined(_WIN32)
        // Show native GUI dialog for fatal application crashes
        if (severity == Severity::fatal)
        {
            std::string msgBoxText = std::format(
                "DNItemMaker has encountered an unhandled error and must close.\n\n"
                "Reason: {}\n\n"
                "A diagnostic crash report and memory dump have been saved:\n"
                "  * Log : {}\n{}\n"
                "Please share these files with developers for debugging.",
                reason,
                reportPath,
                minidumpPath.empty() ? "" : std::format("  * Dump: {}\n", minidumpPath)
            );
            MessageBoxA(nullptr, msgBoxText.c_str(), "DNItemMaker - Application Crash", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        }
#endif
    }

#if defined(_WIN32)
    LONG WINAPI WindowsUnhandledExceptionFilter(EXCEPTION_POINTERS* ep)
    {
        std::lock_guard<std::mutex> lock(g_crashMutex);
        if (g_handlingCrash)
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }
        g_handlingCrash = true;

        std::string timestamp = GetTimestampString();
        uint32_t pid = GetCurrentProcessIdVal();
        std::string minidumpPath = std::format("{}/Crash_{}_{}.dmp", g_dumpDirectory, timestamp, pid);

        std::error_code ec;
        std::filesystem::create_directories(g_dumpDirectory, ec);

        // 1. Write Windows MiniDump
        bool dumpSuccess = WriteMiniDumpFile(minidumpPath, ep);
        if (!dumpSuccess)
        {
            minidumpPath = "(Failed to write minidump)";
        }

        // 2. Gather Exception Details
        DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
        PVOID address = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
        std::string reason = std::format("Win32 SEH Exception 0x{:08X} - {}", code, ExceptionCodeToString(code));

        std::string details;
        if (ep && ep->ExceptionRecord)
        {
            details += std::format("  Faulting Address: 0x{:p}\n", address);
            if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
            {
                ULONG_PTR opType = ep->ExceptionRecord->ExceptionInformation[0];
                ULONG_PTR targetAddr = ep->ExceptionRecord->ExceptionInformation[1];
                const char* opName = (opType == 0) ? "Read" : (opType == 1 ? "Write" : "Execute");
                details += std::format("  Access Type     : {} violation at address 0x{:016X}\n", opName, targetAddr);
            }
        }

        std::string registers = ep && ep->ContextRecord ? FormatRegisters(ep->ContextRecord) : "";
        std::stacktrace trace = std::stacktrace::current();

        // 3. Write Crash Report Log
        GenerateCrashReport(Severity::fatal, reason, details, registers, trace, minidumpPath);

        return EXCEPTION_EXECUTE_HANDLER;
    }

    void CrtInvalidParameterHandler(const wchar_t* expression, const wchar_t* function,
                                    const wchar_t* file, unsigned int line, uintptr_t)
    {
        auto wstrToStr = [](const wchar_t* wstr) -> std::string {
            if (!wstr) return "unknown";
            std::string str;
            while (*wstr) { str.push_back(static_cast<char>(*wstr++)); }
            return str;
        };

        std::string exprStr = wstrToStr(expression);
        std::string funcStr = wstrToStr(function);
        std::string fileStr = wstrToStr(file);

        std::string details = std::format("  Expression: {}\n  Function  : {}\n  Location  : {}:{}\n",
                                          exprStr, funcStr, fileStr, line);

        std::stacktrace trace = std::stacktrace::current();
        GenerateCrashReport(Severity::fatal, "CRT Invalid Parameter Handler invoked", details, "", trace, "");
        std::abort();
    }

    void CrtPureCallHandler()
    {
        std::stacktrace trace = std::stacktrace::current();
        GenerateCrashReport(Severity::fatal, "CRT Pure Virtual Function Call", "A pure virtual method was called on an incomplete object.", "", trace, "");
        std::abort();
    }
#endif

    void PosixSignalHandler(int signum)
    {
        std::lock_guard<std::mutex> lock(g_crashMutex);
        if (g_handlingCrash)
        {
            std::_Exit(EXIT_FAILURE);
        }
        g_handlingCrash = true;

        std::string reason = std::format("Signal {} - {}", signum, SignalToString(signum));
        std::stacktrace trace = std::stacktrace::current();

        GenerateCrashReport(Severity::fatal, reason, "", "", trace, "");

        std::signal(signum, SIG_DFL);
        std::raise(signum);
    }

} // anonymous namespace

    void Initialize(const std::string &dumpDirectory)
    {
        static bool initialized = false;
        if (initialized) return;
        initialized = true;

        g_dumpDirectory = dumpDirectory;
        std::error_code ec;
        std::filesystem::create_directories(g_dumpDirectory, ec);

#if defined(_WIN32)
        // 1. Windows SEH Unhandled Exception Filter
        SetUnhandledExceptionFilter(WindowsUnhandledExceptionFilter);

        // 2. CRT Invalid Parameter & Purecall Handlers
        _set_invalid_parameter_handler(CrtInvalidParameterHandler);
        _set_purecall_handler(CrtPureCallHandler);

        // 3. Disable CRT abort dialog (direct dump without popup)
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
        // 1. POSIX Alternate Signal Stack (handles stack overflow gracefully)
        stack_t altStack{};
        altStack.ss_sp = malloc(SIGSTKSZ * 2);
        altStack.ss_size = SIGSTKSZ * 2;
        altStack.ss_flags = 0;
        if (altStack.ss_sp)
        {
            sigaltstack(&altStack, nullptr);
        }

        // 2. POSIX Signals via sigaction
        struct sigaction sa{};
        sa.sa_handler = PosixSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_ONSTACK | SA_RESETHAND;

        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
        sigaction(SIGFPE,  &sa, nullptr);
        sigaction(SIGILL,  &sa, nullptr);
#if defined(SIGBUS)
        sigaction(SIGBUS,  &sa, nullptr);
#endif
        sigaction(SIGTERM, &sa, nullptr);
#endif

        // Standard C signals
        std::signal(SIGABRT, PosixSignalHandler);
        std::signal(SIGFPE,  PosixSignalHandler);
        std::signal(SIGILL,  PosixSignalHandler);
        std::signal(SIGSEGV, PosixSignalHandler);
        std::signal(SIGTERM, PosixSignalHandler);

        LOG_DEBUG(Channels::system, "Crash & Signal Interception Initialized. Dumps directed to: {}", g_dumpDirectory);
    }

    void CaptureSnapshot(std::string_view reason)
    {
        std::string timestamp = GetTimestampString();
        uint32_t pid = GetCurrentProcessIdVal();
        std::string minidumpPath;

#if defined(_WIN32)
        minidumpPath = std::format("{}/Snapshot_{}_{}.dmp", g_dumpDirectory, timestamp, pid);
        if (!WriteMiniDumpFile(minidumpPath, nullptr))
        {
            minidumpPath.clear();
        }
#endif

        std::stacktrace trace = std::stacktrace::current();
        GenerateCrashReport(Severity::error, std::string(reason), "Diagnostic snapshot requested by application.", "", trace, minidumpPath);
    }

    void TestCrash(int crashType)
    {
        switch (crashType)
        {
        case 0:
        {
            // Null pointer dereference
            volatile int* p = nullptr;
            *p = 42;
            break;
        }
        case 1:
        {
            // Integer divide by zero
            volatile int a = 10;
            volatile int b = 0;
            volatile int c = a / b;
            (void)c;
            break;
        }
        case 2:
        {
            // CRT abort
            std::abort();
            break;
        }
        case 3:
        {
            // Stack overflow
            struct Recurse {
                void Run() { volatile char buf[1024]; Run(); (void)buf; }
            };
            Recurse r;
            r.Run();
            break;
        }
        default:
            std::abort();
        }
    }

} // namespace Common::Crash
