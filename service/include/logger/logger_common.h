#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SocialNetwork {

namespace Logging {

class Logger;
using LoggerConfig  = std::unordered_map<std::string, std::string>;
using LoggerCreator = std::shared_ptr<Logger> (*)(const LoggerConfig&);

static const LoggerConfig default_null_config             = { {"type", "null"}, {"level", "1"} };
static const LoggerConfig default_stdout_uncolored_config = { {"type", "stdout"} };
static const LoggerConfig default_stdout_colored_config   = { {"type", "stdout"}, {"color", "true"} };
static const LoggerConfig default_stderr_uncolored_config = { {"type", "stderr"} };
static const LoggerConfig default_stderr_colored_config   = { {"type", "stderr"}, {"color", "true"} };
static const LoggerConfig default_file_config             = { {"type", "file"}, {"file_name", "app.log"} };

enum class LogLevel : char
{
    LogAlert,   // 0
    LogError,   // 1
    LogWarng,   // 2
    LogInfor,   // 3
    LogDebug,   // 4
    LogTrace    // 5
};

static inline LogLevel to_loglevel(const LoggerConfig& config)
{
    LogLevel code = LogLevel::LogError;
    const auto it = config.find("level");
    if (it != config.end()) {
        const auto& level = it->second;
        if      (level == "0") code = LogLevel::LogAlert;
        else if (level == "1") code = LogLevel::LogError;
        else if (level == "2") code = LogLevel::LogWarng;
        else if (level == "3") code = LogLevel::LogInfor;
        else if (level == "4") code = LogLevel::LogDebug;
        else if (level == "5") code = LogLevel::LogTrace;
    }
    return code;
}

typedef struct enum_hasher_s {
    template <typename T> std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
} LogLevelHasher;

static const std::unordered_map<LogLevel, std::string, LogLevelHasher> uncolored{
    {LogLevel::LogAlert, " [ALERT] "},
    {LogLevel::LogError, " [ERROR] "},
    {LogLevel::LogWarng, " [WARNG] "},
    {LogLevel::LogInfor, " [INFOR] "},
    {LogLevel::LogDebug, " [DEBUG] "},
    {LogLevel::LogTrace, " [TRACE] "}
};

static const std::unordered_map<LogLevel, std::string, LogLevelHasher> colored{
    {LogLevel::LogAlert, " \x1b[36;1m[ALERT]\x1b[0m "}, // cyan
    {LogLevel::LogError, " \x1b[31;1m[ERROR]\x1b[0m "}, // red
    {LogLevel::LogWarng, " \x1b[33;1m[WARNG]\x1b[0m "}, // yellow
    {LogLevel::LogInfor, " \x1b[32;1m[INFOR]\x1b[0m "}, // green
    {LogLevel::LogDebug, " \x1b[34;1m[DEBUG]\x1b[0m "}, // blue
    {LogLevel::LogTrace, " \x1b[37;1m[TRACE]\x1b[0m "}  // white
};

} // namespace Logging

} // namespace SocialNetwork
