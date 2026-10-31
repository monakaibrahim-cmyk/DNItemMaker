#pragma once

/**
 * @file CrashHandler.h
 * @brief Structured Exception Handling (SEH) and minidump crash diagnostics.
 *
 * Provides system-level crash interception, stack trace unwinding, and Windows
 * MiniDumpWriteDump integration. Automatically captures crash dumps and diagnostic
 * logs upon unhandled exceptions, pure virtual function calls, CRT aborts, or
 * invalid parameters.
 */

#include <cstdint>
#include <string>
#include <string_view>

namespace Common::Crash {

/**
 * @brief Installs all system-level crash and exception handlers.
 *
 * Configures:
 * - Windows: SetUnhandledExceptionFilter, CRT abort and invalid parameter handlers.
 * - Posix: SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS signal handlers with alternate stack.
 *
 * @param dumpDirectory Local directory where crash dumps (.dmp) and logs will be written.
 */
void Initialize(const std::string& dumpDirectory = "Logs");

/**
 * @brief Manually captures a diagnostic snapshot without terminating the process.
 * @param reason Descriptive reason for the snapshot.
 */
void CaptureSnapshot(std::string_view reason = "Manual Snapshot");

/**
 * @brief Triggers a controlled fault to test handler interception and minidump generation.
 * @param crashType 0 = Access Violation, 1 = Divide by Zero, 2 = Abort / CRT failure, 3 = Stack Overflow.
 */
void TestCrash(int crashType = 0);

} // namespace Common::Crash
