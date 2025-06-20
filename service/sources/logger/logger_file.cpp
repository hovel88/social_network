#include <fstream>
#include <stdexcept>
#include <filesystem>
#include "logger/logger_file.h"
#include "logger/logger_factory.h"

namespace SocialNetwork {

const std::string Logging::LoggerFile::def_file_name{"app.log"};

Logging::LoggerFile::~LoggerFile()
{
    if (file_) {
        if (file_->is_open()) file_->close();
        delete file_;
    }
};

Logging::LoggerFile::LoggerFile(const LoggerConfig& config)
:   Logger(config)
{
    auto name = config.find("file_name");
    if (name == config.end()) {
        throw std::runtime_error("no output file provided to file logger");
    }
    if (name->second.empty()) {
        file_name_ = def_file_name;
    } else {
        file_name_ = name->second;
    }

    auto interval = config.find("file_reopen_interval");
    if (interval != config.end()) {
        try {
            long int val = std::stol(interval->second);
            if (val <= 0) val = def_reopen_interval;
            reopen_interval_ = std::chrono::seconds(val);
        } catch (...) {
            throw std::runtime_error(interval->second + " is not a valid reopen interval");
        }
    }

    reopen();
}

void Logging::LoggerFile::log(const std::string_view message, const LogLevel level)
{
    log(message, uncolored.find(level)->second);
}

void Logging::LoggerFile::log(const std::string_view message, const std::string_view custom_directive)
{
    std::string output;
    output.reserve(message.length() + 64);
    output.append(Logger::get_timestamp());
    output.append(custom_directive);
    output.append(message);
    output.push_back('\n');
    if (file_) {
        if (file_->is_open()) {
            mutex_.lock();
            *file_ << output;
            file_->flush();
            mutex_.unlock();
        }
    }
    reopen();
}

void Logging::LoggerFile::reopen()
{
    // TODO: use CLOCK_MONOTONIC_COARSE
    auto now = std::chrono::system_clock::now();
    mutex_.lock();
    if (now - last_reopen_ > reopen_interval_) {
        last_reopen_ = now;
        try {
            if (file_) {
                if (file_->is_open()) file_->close();
            }
        }
        catch (...) {}

        try {
            const auto parent_dir = std::filesystem::path(file_name_).parent_path();
            if (!std::filesystem::is_directory(parent_dir)) {
                if (!std::filesystem::create_directories(parent_dir)) {
                    throw std::runtime_error("cannot create directory for log file: " + parent_dir.string());
                }
            }

            if (!file_) {
                file_ = new std::ofstream(file_name_, std::ofstream::out | std::ofstream::app);
            }
            if (file_) {
                if (!file_->is_open()) {
                    file_->open(file_name_, std::ofstream::out | std::ofstream::app);
                    if (file_->fail()) {
                        throw std::runtime_error("cannot create log file: " + file_name_);
                    }
                }
            }

            last_reopen_ = std::chrono::system_clock::now();
        } catch (std::exception& e) {
            try {
                if (file_) {
                    if (file_->is_open()) file_->close();
                }
            }
            catch (...) {}
            mutex_.unlock();
            throw e;
        }
    }
    mutex_.unlock();
}

namespace Logging {

bool file_logger_registered = register_logger("file", [](const LoggerConfig& config)->std::shared_ptr<Logger>
{
    return std::make_shared<LoggerFile>(config);
});

} // namespace Logging

} // namespace SocialNetwork
