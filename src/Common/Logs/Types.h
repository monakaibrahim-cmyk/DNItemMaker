#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace Common::Logging::Types
{
constexpr std::uint32_t hash(std::string_view string) noexcept
{
    std::uint32_t hash = 2166136261U;

    for (char character : string)
    {
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        hash *= 16777619U;
    }

    return hash;
}

enum Severity : std::uint8_t
{
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

constexpr std::string_view operator*(Severity severity)
{
    switch (severity)
    {
    case trace:
        return "TRACE";
    case debug:
        return "DEBUG";
    case info:
        return "INFO";
    case warning:
        return "WARNING";
    case error:
        return "ERROR";
    case fatal:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

constexpr std::ostream& operator<<(std::ostream& os, Severity severity)
{
    return os << *severity;
}

consteval Severity operator""_severity(const char *string, std::size_t length)
{
    std::string_view value{string, length};

    switch (hash(value))
    {
    case hash("trace"):
        return trace;
    case hash("debug"):
        return debug;
    case hash("info"):
        return info;
    case hash("warning"):
        return warning;
    case hash("error"):
        return error;
    case hash("fatal"):
        return fatal;
    default:
        throw "Invalid Severity Literal!";
    }
}

enum Channels : std::uint8_t
{
    system,
    cache,
    sqlite,
    engine,
    dnt,
    graphics
};

constexpr std::string_view operator*(Channels channel)
{
    switch (channel)
    {
    case system:
        return "SYSTEM";
    case cache:
        return "CACHE";
    case sqlite:
        return "SQLITE";
    case engine:
        return "ENGINE";
    case dnt:
        return "DNT";
    case graphics:
        return "GRAPHICS";
    default:
        return "UNKNOWN";
    }
}

consteval Channels operator""_channel(const char *string, std::size_t length)
{
    std::string_view value{string, length};

    switch (hash(value))
    {
    case hash("system"):
        return system;
    case hash("cache"):
        return cache;
    case hash("sqlite"):
        return sqlite;
    case hash("engine"):
        return engine;
    case hash("dnt"):
        return dnt;
    case hash("graphics"):
        return graphics;
    default:
        throw "Invalid Service Literal!";
    }
}

constexpr std::ostream& operator<<(std::ostream& os, Channels channel)
{
    return os << *channel;
}

consteval std::string_view operator""_modifier(const char *string, std::size_t length)
{
    std::string_view value{string, length};

    switch (hash(value))
    {
    case hash("space"):
        return " ";
    case hash("newline"):
        return "\n";
    default:
        throw "Invalid Modifier Literal!";
    }
}
} // namespace Common::Logging::Types
