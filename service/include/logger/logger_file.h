#pragma once

#include "logger/logger_base.h"

namespace SocialNetwork {

namespace Logging {

//
// logger, который направляет вывод в файл (с указанием пути)
// конфигурация может быть следующей:
//  config = { {"type", "file"}, {"file_name", "app.log"}, {"reopen_interval", "10"} }
//  config = { {"type", "file"}, {"file_name", "app.log"} }
// последний вариант задаст дефолтное значение 300 секунд для 'reopen_interval'
// если в конфигурации будет что-то не так - выбросит исключение
//
class LoggerFile : public Logger
{
public:
    virtual ~LoggerFile();
    LoggerFile() = delete;
    LoggerFile(const LoggerFile&) = delete;
    LoggerFile(LoggerFile&&) = delete;
    LoggerFile& operator=(const LoggerFile&) = delete;
    LoggerFile& operator=(LoggerFile&&) = delete;

    explicit LoggerFile(const LoggerConfig& config);

    virtual void log(const std::string_view message, const LogLevel level) override final;
    virtual void log(const std::string_view message, const std::string_view custom_directive = " [TRACE] ") override final;

private:
    static constexpr int def_reopen_interval{300};
    static const std::string def_file_name;

protected:
    std::mutex                              mutex_{};
    std::chrono::seconds                    reopen_interval_{def_reopen_interval};
    std::chrono::system_clock::time_point   last_reopen_{};
    std::string                             file_name_{def_file_name};
    std::ofstream*                          file_{nullptr};

    void reopen();
};

} // namespace Logging

} // namespace SocialNetwork
