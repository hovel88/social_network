#pragma once

#include <stdio.h>
#include <string.h>
#include <ctime>
#include <mutex>
#include <chrono>
#include <iostream>
#include "logger/logger_common.h"

namespace SocialNetwork {

namespace Logging {

//
// базовый класс для logger, от него надо наследоваться и регистрировать новые
// варианты logger в фабрике.
// однако, данный класс может быть использован самостоятельно, как 'null' logger
//
class Logger
{
public:
    virtual ~Logger() = default;
    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    explicit Logger(const LoggerConfig& config)
    :   max_level_(to_loglevel(config)) {};

    bool available(const LogLevel level) const {
        return ( (level > max_level_) ? (false) : (true) );
    }
    virtual void log(const std::string_view message, const LogLevel level);
    virtual void log(const std::string_view message, const std::string_view custom_directive = " [TRACE] ");

protected:
    LogLevel max_level_{LogLevel::LogError};

    // возвращает форматированную строку вида
    // 'yyyy-mm-dd hh:mm:ss.xxxxxx'
    static std::string get_timestamp();
    static std::tm* get_gmtime(const std::time_t* time, std::tm* tm);
};

} // namespace Logging

} // namespace SocialNetwork
