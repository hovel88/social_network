#pragma once

#include <cstdint>
#include <set>
#include <list>
#include <string>
#include <unordered_map>

namespace SocialNetwork {

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

    extern const std::string http_listening;
    extern const uint16_t    http_port;
    extern const int         http_threads_count;
    extern const int         http_queue_capacity;

    extern const std::string prometheus_listening;
    extern const std::string prometheus_host;
    extern const uint16_t    prometheus_port;

    extern const std::unordered_map<std::string, std::string> logging_config;

} // namespace config_def

namespace config_min {

    extern const int http_threads_count;
    extern const int http_queue_capacity;

} // namespace config_min

namespace config_data {

    struct config_s {

        struct pgsql_s {
            std::string url;
            std::string login;
            std::string password;
        };

        pgsql_s              pgsql_master;
        std::vector<pgsql_s> pgsql_replica;

        std::string http_listening;
        int         http_threads_count;
        int         http_queue_capacity;

        std::string prometheus_listening;
        int         prometheus_port;

        std::unordered_map<std::string, std::string> logging_config;

        void init();
        void clear();
        std::list<std::string> validate();
    };
} // namespace config_data

} // namespace SocialNetwork
