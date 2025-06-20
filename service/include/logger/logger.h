#pragma once

#include "logger/logger_base.h"

namespace SocialNetwork {

namespace Logging {

std::shared_ptr<Logger> configure_logger(const LoggerConfig& config = default_stdout_colored_config);

// можно обращаться к методам logger напрямую из указателя, либо применять
// макросы: LOG_ERROR, LOG_WARNG, LOG_INFOR, LOG_DEBUG, LOG_TRACE
// в случае макросов, можно будет использовать условия препроцессора,
// например, чтобы полностью исключить из компиляции лог определенных уровней

// можно на этапе компиляции задать уровень логирования, который вообще возможен,
// все уровни выше - даже не будут присутствовать в коде, ускоряя работу.
// для этого во флагах компиляции надо перечислить какие логи нужно оставить:
//      LOGGING_LEVEL_ENABLE_ERROR -- при компиляции оставить логи, помеченные как LOG_ERROR 1
//      LOGGING_LEVEL_ENABLE_WARNG -- при компиляции оставить логи, помеченные как LOG_WARNG 2
//      LOGGING_LEVEL_ENABLE_INFOR -- при компиляции оставить логи, помеченные как LOG_INFOR 3
//      LOGGING_LEVEL_ENABLE_DEBUG -- при компиляции оставить логи, помеченные как LOG_DEBUG 4
//      LOGGING_LEVEL_ENABLE_TRACE -- при компиляции оставить логи, помеченные как LOG_TRACE 5
// либо указать одну из опций:
//      LOGGING_LEVEL_ENABLE_NONE  -- при компиляции убрать весь лог из кода
//      LOGGING_LEVEL_ENABLE_ALL   -- при компиляции оставить весь возможный лог в коде

// #define LOGGING_LEVEL_ENABLE_ALL

#if   defined(LOGGING_LEVEL_ENABLE_NONE)
    #undef LOGGING_LEVEL_ENABLE_ALERT
    #undef LOGGING_LEVEL_ENABLE_ERROR
    #undef LOGGING_LEVEL_ENABLE_WARNG
    #undef LOGGING_LEVEL_ENABLE_INFOR
    #undef LOGGING_LEVEL_ENABLE_DEBUG
    #undef LOGGING_LEVEL_ENABLE_TRACE
#elif defined(LOGGING_LEVEL_ENABLE_ALL)
    #define LOGGING_LEVEL_ENABLE_ALERT
    #define LOGGING_LEVEL_ENABLE_ERROR
    #define LOGGING_LEVEL_ENABLE_WARNG
    #define LOGGING_LEVEL_ENABLE_INFOR
    #define LOGGING_LEVEL_ENABLE_DEBUG
    #define LOGGING_LEVEL_ENABLE_TRACE
#else
    #define LOGGING_LEVEL_ENABLE_ERROR
#endif

// C++14 constexpr function can use loop and local variable.
// Constexpr can do this at compile time!
constexpr const char* _file_name_(const char* path) {
    const char* file = path;
    while (*path) {
        if (*path++ == '/') {
            file = path;
        }
    }
    return file;
}

#define LOGGING_LN_PREFIX (std::string("(") + SocialNetwork::Logging::_file_name_(__FILE__) + ":" + std::to_string(__LINE__) + std::string(") :: "))

#ifndef LOGGING_LEVEL_ENABLE_ALERT
    #define LOGMSG_ALERT(msg)
    #define LOGLNG_ALERT(msg)
#else
    #define LOGMSG_ALERT(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogAlert)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogAlert)
    #define LOGLNG_ALERT(msg) LOGMSG_ALERT((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_ALERT(msg) LOGLNG_ALERT(msg)

#ifndef LOGGING_LEVEL_ENABLE_ERROR
    #define LOGMSG_ERROR(msg)
    #define LOGLNG_ERROR(msg)
#else
    #define LOGMSG_ERROR(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogError)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogError)
    #define LOGLNG_ERROR(msg) LOGMSG_ERROR((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_ERROR(msg) LOGLNG_ERROR(msg)

#ifndef LOGGING_LEVEL_ENABLE_WARNG
    #define LOGMSG_WARNG(msg)
    #define LOGLNG_WARNG(msg)
#else
    #define LOGMSG_WARNG(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogWarng)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogWarng)
    #define LOGLNG_WARNG(msg) LOGMSG_WARNG((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_WARNG(msg) LOGLNG_WARNG(msg)

#ifndef LOGGING_LEVEL_ENABLE_INFOR
    #define LOGMSG_INFOR(msg)
    #define LOGLNG_INFOR(msg)
#else
    #define LOGMSG_INFOR(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogInfor)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogInfor)
    #define LOGLNG_INFOR(msg) LOGMSG_INFOR((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_INFOR(msg) LOGLNG_INFOR(msg)

#ifndef LOGGING_LEVEL_ENABLE_DEBUG
    #define LOGMSG_DEBUG(msg)
    #define LOGLNG_DEBUG(msg)
#else
    #define LOGMSG_DEBUG(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogDebug)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogDebug)
    #define LOGLNG_DEBUG(msg) LOGMSG_DEBUG((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_DEBUG(msg) LOGLNG_DEBUG(msg)

#ifndef LOGGING_LEVEL_ENABLE_TRACE
    #define LOGMSG_TRACE(msg)
    #define LOGLNG_TRACE(msg)
#else
    #define LOGMSG_TRACE(msg) if (logger_ && logger_->available(SocialNetwork::Logging::LogLevel::LogTrace)) logger_->log(msg, SocialNetwork::Logging::LogLevel::LogTrace)
    #define LOGLNG_TRACE(msg) LOGMSG_TRACE((LOGGING_LN_PREFIX + msg))
#endif
#define LOG_TRACE(msg) LOGLNG_TRACE(msg)

} // namespace Logging

} // namespace SocialNetwork
