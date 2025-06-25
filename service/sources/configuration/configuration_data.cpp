#include <format>
#include <iostream>
#include "helpers/url.h"
#include "helpers/ip_address.h"
#include "helpers/dns_address.h"
#include "helpers/socket_address.h"
#include "configuration/configuration_data.h"

namespace SocialNetwork {

const std::string config_def::pgsql_endpoint{"postgresql://localhost:5432/postgres"};
const std::string config_def::pgsql_database{"postgres"};
const std::string config_def::pgsql_login{"postgres"};
const std::string config_def::pgsql_password{""};
const uint16_t config_def::pgsql_port = 5432;

const std::string config_def::http_listening{"0.0.0.0:6000"};
const uint16_t config_def::http_port = 6000;

const int config_max::http_threads_count = 10;
const int config_def::http_threads_count = 1;
const int config_min::http_threads_count = 1;

const int config_max::http_queue_capacity = 4096;
const int config_def::http_queue_capacity = 1024;
const int config_min::http_queue_capacity = 1;

const std::string config_def::prometheus_listening{"0.0.0.0:6001"};
const std::string config_def::prometheus_host{"0.0.0.0"};
const uint16_t    config_def::prometheus_port = 6001;

const std::unordered_map<std::string, std::string> config_def::logging_config = { {"type", "stdout"}, {"color", "true"}, {"level", "1"} };



// --------------------------------------------------------

void config_data::config_s::init()
{
    clear();
}

void config_data::config_s::clear()
{
    pgsql_endpoint      = config_def::pgsql_endpoint;
    pgsql_login.clear();
    pgsql_password.clear();

    http_listening      = config_def::http_listening;
    http_threads_count  = config_def::http_threads_count;
    http_queue_capacity = config_def::http_queue_capacity;

    prometheus_listening = config_def::prometheus_listening;

    logging_config = config_def::logging_config;
}

std::list<std::string> config_data::config_s::validate()
{
    std::list<std::string> errors{};

    try {
        if (pgsql_endpoint.find("://") == std::string::npos) {
            auto endpoint_with_scheme = std::format("postgresql://{}", pgsql_endpoint);
            pgsql_endpoint.swap(endpoint_with_scheme);
        }
        UrlHelpers::Url url(pgsql_endpoint);

        // верификация схемы URL
        std::string scheme = url.get_scheme();
        if (!scheme.empty() && scheme != "postgresql")
            throw std::runtime_error(std::format("scheme should be 'postgresql'"));

        // верификация номера порта
        NetHelpers::SocketAddress sock_addr(std::format("{}:{}", url.get_host(), url.get_specified_port()));
        if (sock_addr.port() == 0) url.set_port(config_def::pgsql_port);

        // верификация сегментов пути URL
        std::vector<std::string> segments;
        url.set_fragment("");
        url.get_path_segments(segments);
        if (segments.empty()) url.set_path(config_def::pgsql_database);

        // верификация "login:password" части URL
        // переупаковываем URL, убирая "чувствительную" часть
        std::string login;
        std::string password;
        std::string userinfo = url.get_user_info(); // "login:password"
        if (!userinfo.empty()) {
            auto colon = userinfo.find(':');
            if (colon != std::string::npos) {
                login    = userinfo.substr(0, colon);
                password = userinfo.substr(colon+1);
            } else {
                login = userinfo;
            }
        }
        if (pgsql_login.empty()) {
            pgsql_login = !login.empty() ? login : config_def::pgsql_login;
        }
        if (pgsql_password.empty()) {
            pgsql_password = !password.empty() ? password : config_def::pgsql_password;
        }
        url.set_user_info("");
        pgsql_endpoint = url.to_string();
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
            pgsql_endpoint, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
            pgsql_endpoint, ex.what()));
        pgsql_endpoint = config_def::pgsql_endpoint;
    }

    if (pgsql_login.empty())    pgsql_login    = config_def::pgsql_login;
    if (pgsql_password.empty()) pgsql_password = config_def::pgsql_password;

    try {
        NetHelpers::SocketAddress sock_addr(http_listening);
        if (sock_addr.port() == 0) {
            http_listening = std::format("{}:{}", sock_addr.host().to_string(), config_def::http_port);
        }
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'http.listening={}': {}",
            http_listening, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'http.listening={}': {}",
            http_listening, ex.what()));
        http_listening.assign(config_def::http_listening);
    }

    if (http_threads_count < config_min::http_threads_count
    ||  http_threads_count > config_max::http_threads_count) {
        errors.push_back(std::format("validation error 'http.threads_count={}': should be in range [{}..{}]",
            http_threads_count, config_min::http_threads_count, config_max::http_threads_count));
        http_threads_count = config_def::http_threads_count;
    }

    if (http_queue_capacity < config_min::http_queue_capacity
    ||  http_queue_capacity > config_max::http_queue_capacity) {
        errors.push_back(std::format("validation error 'http.queue_capacity={}': should be in range [{}..{}]",
            http_queue_capacity, config_min::http_queue_capacity, config_max::http_queue_capacity));
        http_queue_capacity = config_def::http_queue_capacity;
    }

    try {
        prometheus_listening = std::format("{}:{}", config_def::prometheus_host, prometheus_port);
        NetHelpers::SocketAddress sock_addr(prometheus_listening);
        if (sock_addr.port() == 0) {
            prometheus_listening = std::format("{}:{}", config_def::prometheus_host, config_def::prometheus_port);
        }
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'prometheus.listening={}': {}",
            prometheus_listening, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'prometheus.listening={}': {}",
            prometheus_listening, ex.what()));
        prometheus_listening.assign(config_def::prometheus_listening);
    }

    return errors;
}

} // namespace SocialNetwork
