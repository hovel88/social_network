#pragma once

#include <cstdint>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <unordered_map>
#include "logger/logger.h"

namespace config_data {
    namespace config_max {

        extern const int http_threads_count;
        extern const int http_queue_capacity;

    } // namespace config_max

    namespace config_def {

        extern const std::string kafka_url;
        extern const uint16_t    kafka_port;

        extern const std::string http_listening;
        extern const uint16_t    http_port;
        extern const int         http_threads_count;
        extern const int         http_queue_capacity;

        extern const std::string prometheus_listening;
        extern const std::string prometheus_host;
        extern const uint16_t    prometheus_port;

    } // namespace config_def

    namespace config_min {

        extern const int http_threads_count;
        extern const int http_queue_capacity;

    } // namespace config_min

    struct config_s {

        std::string kafka_url;

        std::string http_listening;
        int         http_threads_count;
        int         http_queue_capacity;

        std::string prometheus_listening;
        int         prometheus_port;

        void clear();
        std::list<std::string> validate();
    };

} // namespace config_data


class Configuration : public config_data::config_s
{
public:
    ~Configuration() = default;
    Configuration() = delete;
    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) = delete;
    Configuration& operator=(const Configuration&) = delete;
    Configuration& operator=(Configuration&&) = delete;

    static const Configuration& instance();

    std::shared_ptr<Logging::Logger> get_logger() const      { return logger_; }
    void set_logger(std::shared_ptr<Logging::Logger> logger) { logger_ = std::move(logger); }

    void show_configuration() const;

private:
    explicit Configuration(std::shared_ptr<Logging::Logger> logger);

    std::shared_ptr<Logging::Logger> logger_{nullptr};
};
