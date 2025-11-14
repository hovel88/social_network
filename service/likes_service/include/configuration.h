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

        extern const std::string pgsql_url;
        extern const uint16_t    pgsql_port;
        extern const std::string pgsql_database;
        extern const std::string pgsql_login;
        extern const std::string pgsql_password;

        extern const std::string grpc_url;
        extern const uint16_t    grpc_port;

        extern const std::string kafka_url;
        extern const uint16_t    kafka_port;

        extern const std::string http_listening;
        extern const uint16_t    http_port;
        extern const int         http_threads_count;
        extern const int         http_queue_capacity;

    } // namespace config_def

    namespace config_min {

        extern const int http_threads_count;
        extern const int http_queue_capacity;

    } // namespace config_min

    struct config_s {

        struct pgsql_s {
            std::string url;
            std::string login;
            std::string password;
        };

        pgsql_s pgsql_master;
        pgsql_s pgsql_replica;

        std::string grpc_url;

        std::string kafka_url;

        std::string http_listening;
        int         http_threads_count;
        int         http_queue_capacity;

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

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
};
