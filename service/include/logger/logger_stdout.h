#pragma once

#include "logger/logger_base.h"

namespace SocialNetwork {

namespace Logging {

//
// logger, который направляет вывод в std::cout
// конфигурация может быть следующей:
//  config = { {"type", "std_out"}, {"color", "true"} }
//  config = { {"type", "std_out"}, {"color", "false"} }
//  config = { {"type", "std_out"} }
// последние два варианта конфигурации идентичны
// если в конфигурации будет что-то не так - выбросит исключение
//
class LoggerStdOut : public Logger
{
public:
    virtual ~LoggerStdOut() {};
    LoggerStdOut() = delete;
    LoggerStdOut(const LoggerStdOut&) = delete;
    LoggerStdOut(LoggerStdOut&&) = delete;
    LoggerStdOut& operator=(const LoggerStdOut&) = delete;
    LoggerStdOut& operator=(LoggerStdOut&&) = delete;

    explicit LoggerStdOut(const LoggerConfig& config)
    :   Logger(config),
        levels_(config.find("color") != config.end() && config.find("color")->second == "true" ? colored : uncolored) {}

    virtual void log(const std::string_view message, const LogLevel level) override final;
    virtual void log(const std::string_view message, const std::string_view custom_directive = " [TRACE] ") override final;

protected:
    const std::unordered_map<LogLevel, std::string, LogLevelHasher> levels_;
};

} // namespace Logging

} // namespace SocialNetwork
