#pragma once

/**
 * @file Logs.h
 * @brief Thread-safe logging subsystem supporting compile-time std::format validation.
 *
 * Implements a high-throughput, thread-safe diagnostic logger with structured severity
 * levels, channel filtering, millisecond timestamps, thread IDs, and automated source
 * location capture (__FILE__, __LINE__, __FUNCSIG__).
 */

#include <Common/Logs/Types.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#ifdef _MSC_VER
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#else
    #include <unistd.h>
#endif

#define __LOG_DIRECTORY__       "Logs"
#define __LOG_FILE__            "DNItemMaker.log"

using namespace Common::Logging::Types;

namespace Common
{
    /**
     * @brief Source code origin captured at log call sites.
     */
    struct LogLocation
    {
        const char *file = "";      ///< Source filename.
        uint32_t line = 0;          ///< 1-based line number.
        const char *function = "";  ///< Function or method signature.
    };

    /**
     * @class Logs
     * @brief Thread-safe singleton logging manager writing formatted messages to disk and console.
     */
    class Logs
    {
    public:
        /**
         * @brief Initializes the logging subsystem with target directory and filename.
         * @param directory Output folder path (e.g. "Logs").
         * @param filename Log filename (e.g. "DNItemMaker.log").
         */
        static void Initialize(const std::string &directory, const std::string &filename);

        /** @brief Retrieves the active singleton Logs instance. */
        static Logs &Instance();

        Logs(std::string directory, std::string filename);
        ~Logs();

        Logs(const Logs &) = delete;
        Logs &operator=(const Logs &) = delete;
        Logs(Logs &&) = delete;
        Logs &operator=(Logs &&) = delete;

        /**
         * @brief Strips project path prefixes to produce clean, relative source paths.
         * @param path Full source file path.
         * @return Relative source path view.
         */
        static constexpr std::string_view StripFilePath(std::string_view path) noexcept
        {
            if (path.empty())
            {
                return {};
            }
            constexpr std::string_view markers[] = {
                "src\\", "src/", "source\\", "source/", "include\\", "include/", "includes\\", "includes/"
            };
            for (const auto marker : markers)
            {
                const auto pos = path.find(marker);
                if (pos != std::string_view::npos && (pos == 0 || path[pos - 1] == '\\' || path[pos - 1] == '/'))
                {
                    return path.substr(pos);
                }
            }
            return path;
        }

        /**
         * @brief Low-level write implementation outputting formatted records to file and debug output.
         * @param severity Log message severity level.
         * @param channel Logging subsystem channel.
         * @param loc Call site source code location.
         * @param message Text message content.
         */
        void WriteLog(Severity severity, Channels channel, LogLocation loc, std::string_view message);

        /**
         * @brief Formatted log write taking compile-time validated format string and variadic arguments.
         */
        template <typename... Args>
        void Write(Severity severity, Channels channel, LogLocation loc,
                   std::format_string<Args...> fmt, Args &&...args)
        {
            const std::string formatted = std::format(fmt, std::forward<Args>(args)...);
            WriteLog(severity, channel, loc, formatted);
        }

        /** @brief Write raw string message with source location. */
        void Write(Severity severity, Channels channel, LogLocation loc, std::string_view message)
        {
            WriteLog(severity, channel, loc, message);
        }

        /** @brief Write formatted message without explicit source location. */
        template <typename... Args>
        void Write(Severity severity, Channels channel, std::format_string<Args...> fmt, Args &&...args)
        {
            const std::string formatted = std::format(fmt, std::forward<Args>(args)...);
            WriteLog(severity, channel, {}, formatted);
        }

        /** @brief Write raw string message without source location. */
        void Write(Severity severity, Channels channel, std::string_view message)
        {
            WriteLog(severity, channel, {}, message);
        }

        /** @brief Backward-compatibility overload accepting separate file, line, function parameters. */
        template <typename... Args>
        void Write(Severity severity, Channels channel, const std::string &file, uint32_t line, const std::string &function,
                   std::format_string<Args...> fmt, Args &&...args)
        {
            Write(severity, channel, LogLocation{file.c_str(), line, function.c_str()}, fmt, std::forward<Args>(args)...);
        }

        /** @brief Backward-compatibility string overload. */
        void Write(Severity severity, Channels channel, const std::string &file, uint32_t line, const std::string &function,
                   std::string_view message)
        {
            Write(severity, channel, LogLocation{file.c_str(), line, function.c_str()}, message);
        }

        /**
         * @brief High-priority error/fatal write with compile-time format validation.
         */
        template <typename... Args>
        void WriteF(Severity severity, Channels channel, LogLocation loc,
                    std::format_string<Args...> fmt, Args &&...args)
        {
            if (severity != Severity::error && severity != Severity::fatal)
            {
                return;
            }
            const std::string formatted = std::format(fmt, std::forward<Args>(args)...);
            WriteLog(severity, channel, loc, formatted);
        }

        /** @brief High-priority error/fatal write with raw string. */
        void WriteF(Severity severity, Channels channel, LogLocation loc, std::string_view message)
        {
            if (severity != Severity::error && severity != Severity::fatal)
            {
                return;
            }
            WriteLog(severity, channel, loc, message);
        }

        /** @brief High-priority error/fatal write without source location. */
        template <typename... Args>
        void WriteF(Severity severity, Channels channel, std::format_string<Args...> fmt, Args &&...args)
        {
            if (severity != Severity::error && severity != Severity::fatal)
            {
                return;
            }
            const std::string formatted = std::format(fmt, std::forward<Args>(args)...);
            WriteLog(severity, channel, {}, formatted);
        }

        /** @brief High-priority error/fatal raw string write without source location. */
        void WriteF(Severity severity, Channels channel, std::string_view message)
        {
            if (severity != Severity::error && severity != Severity::fatal)
            {
                return;
            }
            WriteLog(severity, channel, {}, message);
        }

        template <typename... Args>
        void WriteF(Severity severity, Channels channel, const std::string &file, uint32_t line, const std::string &function,
                    std::format_string<Args...> fmt, Args &&...args)
        {
            WriteF(severity, channel, LogLocation{file.c_str(), line, function.c_str()}, fmt, std::forward<Args>(args)...);
        }

        void WriteF(Severity severity, Channels channel, const std::string &file, uint32_t line, const std::string &function,
                    std::string_view message)
        {
            WriteF(severity, channel, LogLocation{file.c_str(), line, function.c_str()}, message);
        }

        /** @brief Flushes pending stream buffers to disk. */
        void Flush();

        /** @brief Shuts down logging and releases file locks. */
        static void Shutdown();

    private:
        std::string szFileName;
        std::string szDirectory;
        std::filesystem::path szFullPath;
        std::ofstream szFileStream;

        static std::unique_ptr<Logs> instance;

        std::mutex szLogMutex;
        std::error_code szErrorCode;
    };
} // namespace Common

#ifdef _MSC_VER
    #define __FUNC__ __FUNCSIG__
#else
    #define __FUNC__ __PRETTY_FUNCTION__
#endif

/** @brief Log trace message with compile-time std::format string and caller location. */
#define LOG_TRACE(channel, fmt, ...)                                                                                    \
    ::Common::Logs::Instance().Write(Severity::trace, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)

/** @brief Log debug message with compile-time std::format string and caller location. */
#define LOG_DEBUG(channel, fmt, ...)                                                                                    \
    ::Common::Logs::Instance().Write(Severity::debug, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)

/** @brief Log informational message with compile-time std::format string and caller location. */
#define LOG_INFO(channel, fmt, ...)                                                                                     \
    ::Common::Logs::Instance().Write(Severity::info, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)

/** @brief Log warning message with compile-time std::format string and caller location. */
#define LOG_WARNING(channel, fmt, ...)                                                                                  \
    ::Common::Logs::Instance().Write(Severity::warning, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)

/** @brief Log error message with compile-time std::format string and caller location. */
#define LOG_ERROR(channel, fmt, ...)                                                                                    \
    ::Common::Logs::Instance().WriteF(Severity::error, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)

/** @brief Log fatal error message with compile-time std::format string and caller location. */
#define LOG_FATAL(channel, fmt, ...)                                                                                    \
    ::Common::Logs::Instance().WriteF(Severity::fatal, channel, ::Common::LogLocation{__FILE__, __LINE__, __FUNC__}, fmt, ##__VA_ARGS__)
