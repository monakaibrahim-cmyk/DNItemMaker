#include <Common/Logs/Logs.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <utility>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#endif

namespace Common
{
std::unique_ptr<Logs> Logs::instance = nullptr;

Logs::Logs(std::string directory, std::string filename)
    : szFileName(std::move(filename)), szDirectory(std::move(directory))
{
    szFullPath = std::filesystem::path(szDirectory) / szFileName;

    if (!std::filesystem::exists(szDirectory, szErrorCode))
    {
        std::filesystem::create_directories(szDirectory, szErrorCode);
    }

    szFileStream.open(szFullPath, std::ios::out | std::ios::app);
}

Logs::~Logs()
{
    if (szFileStream.is_open())
    {
        szFileStream.close();
    }
}

void Logs::Initialize(const std::string &directory, const std::string &filename)
{
    if (instance != nullptr)
    {
        return;
    }

    instance = std::make_unique<Logs>(directory, filename);
}

Logs &Logs::Instance()
{
    if (instance == nullptr)
    {
        Initialize(__LOG_DIRECTORY__, __LOG_FILE__);
    }

    return *instance;
}

void Logs::WriteLog(Severity severity, Channels channel, LogLocation loc, std::string_view message)
{
    if (severity > Severity::fatal)
    {
        return;
    }

    char tsBuf[32];
    int tsLen = 0;
#if defined(_WIN32)
    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    tsLen = std::snprintf(tsBuf, sizeof(tsBuf), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                          st.wYear, st.wMonth, st.wDay,
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    tsLen = std::snprintf(tsBuf, sizeof(tsBuf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                          tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
#endif
    const std::string_view timestamp(tsBuf, tsLen > 0 ? static_cast<size_t>(tsLen) : 0);

    std::string line;
    line.reserve(128 + message.size());
    line.append(timestamp);
    line.append(" [");
    line.append(*severity);
    line.append("] [");
    line.append(*channel);
    line.append("] ");

    if (loc.file && (loc.file[0] != '\0' || loc.line > 0 || (loc.function && loc.function[0] != '\0')))
    {
        line.push_back('[');
        line.append(StripFilePath(loc.file));
        line.push_back(':');
        line.append(std::to_string(loc.line));
        line.append("] [");
        if (loc.function)
        {
            line.append(loc.function);
        }
        line.append("] ");
    }

    line.append(": ");
    line.append(message);
    line.push_back('\n');

    std::scoped_lock lock(szLogMutex);

    if (!szFileStream.is_open())
    {
        szFileStream.open(szFullPath, std::ios::out | std::ios::app);
    }

    if (szFileStream.is_open())
    {
        szFileStream.write(line.data(), static_cast<std::streamsize>(line.size()));
        szFileStream.flush();
    }
}

void Logs::Flush()
{
    std::scoped_lock lock(szLogMutex);
    if (szFileStream.is_open())
    {
        szFileStream.flush();
    }
}

void Logs::Shutdown()
{
    instance.reset();
}

} // namespace Common
