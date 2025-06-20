#pragma once

#include "logger/logger_base.h"

namespace SocialNetwork {

namespace Logging {

//
// logger, который направляет вывод в std::cerr
// конфигурация может быть следующей:
//  config = { {"type", "std_err"}, {"color", "true"} }
//  config = { {"type", "std_err"}, {"color", "false"} }
//  config = { {"type", "std_err"} }
// последние два варианта конфигурации идентичны
// если в конфигурации будет что-то не так - выбросит исключение
//
class LoggerStdErr : public Logger
{
public:
    virtual ~LoggerStdErr() {};
    LoggerStdErr() = delete;
    LoggerStdErr(const LoggerStdErr&) = delete;
    LoggerStdErr(LoggerStdErr&&) = delete;
    LoggerStdErr& operator=(const LoggerStdErr&) = delete;
    LoggerStdErr& operator=(LoggerStdErr&&) = delete;

    explicit LoggerStdErr(const LoggerConfig& config)
    :   Logger(config),
        levels_(config.find("color") != config.end() && config.find("color")->second == "true" ? colored : uncolored) {}

    virtual void log(const std::string_view message, const LogLevel level) override final;
    virtual void log(const std::string_view message, const std::string_view custom_directive = " [TRACE] ") override final;

protected:
    const std::unordered_map<LogLevel, std::string, LogLevelHasher> levels_;
};

} // namespace Logging

} // namespace SocialNetwork
