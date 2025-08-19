#include <format>
#include <iostream>
#include "helpers/environment.h"
#include "helpers/string.h"
#include "helpers/number_parser.h"
#include "helpers/url.h"
#include "helpers/ip_address.h"
#include "helpers/dns_address.h"
#include "helpers/socket_address.h"
#include "configuration.h"

std::string print_collection(const auto& seq) {
    std::stringstream ss;
    ss << "[";
    for (auto n{seq.size()}; const auto& elm : seq)
        ss << elm << (--n ? ", " : "");
    ss << "]";
    return ss.str();
};

// ----------------------------------------------------------------------------

const std::string config_data::config_def::pgsql_url{"postgresql://localhost:5432/postgres"};
const std::string config_data::config_def::pgsql_database{"postgres"};
const std::string config_data::config_def::pgsql_login{"postgres"};
const std::string config_data::config_def::pgsql_password{""};
const uint16_t    config_data::config_def::pgsql_port = 5432;

const std::string config_data::config_def::grpc_url{"grpc://localhost:50051"};
const uint16_t    config_data::config_def::grpc_port = 50051;

const std::string config_data::config_def::kafka_url{"tcp://localhost:9092"};
const uint16_t    config_data::config_def::kafka_port = 9092;

const std::string config_data::config_def::http_listening{"0.0.0.0:6000"};
const uint16_t    config_data::config_def::http_port = 6000;

const int config_data::config_max::http_threads_count = 10;
const int config_data::config_def::http_threads_count = 1;
const int config_data::config_min::http_threads_count = 1;

const int config_data::config_max::http_queue_capacity = 100000;
const int config_data::config_def::http_queue_capacity = 1024;
const int config_data::config_min::http_queue_capacity = 1;

const std::string config_data::config_def::prometheus_listening{"0.0.0.0:6001"};
const std::string config_data::config_def::prometheus_host{"0.0.0.0"};
const uint16_t    config_data::config_def::prometheus_port = 6001;



void config_data::config_s::clear()
{
    pgsql_master.url = config_def::pgsql_url;
    pgsql_master.login.clear();
    pgsql_master.password.clear();

    pgsql_replica.clear();

    grpc_url = config_def::grpc_url;

    kafka_url = config_def::kafka_url;

    http_listening      = config_def::http_listening;
    http_threads_count  = config_def::http_threads_count;
    http_queue_capacity = config_def::http_queue_capacity;

    prometheus_listening = config_def::prometheus_listening;
    prometheus_port      = config_def::prometheus_port;
}

std::list<std::string> config_data::config_s::validate()
{
    std::list<std::string> errors{};

    try {
        if (pgsql_master.url.find("://") == std::string::npos) {
            auto endpoint_with_scheme = std::format("postgresql://{}", pgsql_master.url);
            pgsql_master.url.swap(endpoint_with_scheme);
        }
        UrlHelpers::Url url(pgsql_master.url);

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
        if (pgsql_master.login.empty()) {
            pgsql_master.login = !login.empty() ? login : config_def::pgsql_login;
        }
        if (pgsql_master.password.empty()) {
            pgsql_master.password = !password.empty() ? password : config_def::pgsql_password;
        }
        url.set_user_info("");
        pgsql_master.url = url.to_string();
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
            pgsql_master.url, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
            pgsql_master.url, ex.what()));
        pgsql_master.url = config_def::pgsql_url;
    }

    if (pgsql_master.login.empty())    pgsql_master.login    = config_def::pgsql_login;
    if (pgsql_master.password.empty()) pgsql_master.password = config_def::pgsql_password;

    for (auto& replica : pgsql_replica) {
        try {
            if (replica.url.find("://") == std::string::npos) {
                auto endpoint_with_scheme = std::format("postgresql://{}", replica.url);
                replica.url.swap(endpoint_with_scheme);
            }
            UrlHelpers::Url url(replica.url);

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
            if (replica.login.empty()) {
                replica.login = !login.empty() ? login : config_def::pgsql_login;
            }
            if (replica.password.empty()) {
                replica.password = !password.empty() ? password : config_def::pgsql_password;
            }
            url.set_user_info("");
            replica.url = url.to_string();
        }
        catch (NetHelpers::DnsException& ex) {
            errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
                replica.url, ex.what()));
        }
        catch (std::exception& ex) {
            errors.push_back(std::format("validation error 'pgsql.endpoint={}': {}",
                replica.url, ex.what()));
            replica.url = config_def::pgsql_url;
        }

        if (replica.login.empty())    replica.login    = config_def::pgsql_login;
        if (replica.password.empty()) replica.password = config_def::pgsql_password;
    }

    try {
        if (grpc_url.find("://") == std::string::npos) {
            auto endpoint_with_scheme = std::format("grpc://{}", grpc_url);
            grpc_url.swap(endpoint_with_scheme);
        }
        UrlHelpers::Url url(grpc_url);

        // верификация схемы URL
        std::string scheme = url.get_scheme();
        if (!scheme.empty() && scheme != "grpc")
            throw std::runtime_error(std::format("scheme should be 'grpc'"));

        // верификация номера порта
        NetHelpers::SocketAddress sock_addr(std::format("{}:{}", url.get_host(), url.get_specified_port()));
        if (sock_addr.port() == 0) url.set_port(config_def::grpc_port);

        // верификация сегментов пути URL
        url.set_fragment("");
        url.set_path("");

        // верификация "login:password" части URL
        // переупаковываем URL, убирая "чувствительную" часть
        url.set_user_info("");
        grpc_url = url.to_string();
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'grpc.url={}': {}",
            grpc_url, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'grpc.url={}': {}",
            grpc_url, ex.what()));
        grpc_url = config_def::grpc_url;
    }

    try {
        if (kafka_url.find("://") == std::string::npos) {
            auto endpoint_with_scheme = std::format("tcp://{}", kafka_url);
            kafka_url.swap(endpoint_with_scheme);
        }
        UrlHelpers::Url url(kafka_url);

        // верификация схемы URL
        std::string scheme = url.get_scheme();
        if (!scheme.empty() && scheme != "tcp")
            throw std::runtime_error(std::format("scheme should be 'tcp'"));

        // верификация номера порта
        NetHelpers::SocketAddress sock_addr(std::format("{}:{}", url.get_host(), url.get_specified_port()));
        if (sock_addr.port() == 0) url.set_port(config_def::kafka_port);

        // верификация сегментов пути URL
        url.set_fragment("");
        url.set_path("");

        // верификация "login:password" части URL
        // переупаковываем URL, убирая "чувствительную" часть
        url.set_user_info("");
        kafka_url = url.to_string();
    }
    catch (NetHelpers::DnsException& ex) {
        errors.push_back(std::format("validation error 'kafka.url={}': {}",
            kafka_url, ex.what()));
    }
    catch (std::exception& ex) {
        errors.push_back(std::format("validation error 'kafka.url={}': {}",
            kafka_url, ex.what()));
        kafka_url = config_def::kafka_url;
    }

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

// ----------------------------------------------------------------------------


Configuration::Configuration(std::shared_ptr<Logging::Logger> logger)
:   logger_(std::move(logger))
{
    clear();
    LOGGER_INFOR(std::format("configuration was initialized with default values"));

    LOGGER_INFOR(std::format("trying to read configuration from environment"));

    {
        const std::string key("PGSQL_MASTER_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            pgsql_master.url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_1_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            pgsql_replica.push_back({});
            pgsql_replica.back().url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_2_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            pgsql_replica.push_back({});
            pgsql_replica.back().url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_3_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            pgsql_replica.push_back({});
            pgsql_replica.back().url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }

    {
        const std::string key("GRPC_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            grpc_url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }

    {
        const std::string key("KAFKA_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            kafka_url = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }

    {
        const std::string key("HTTP_LISTENING");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            http_listening = val;
            LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("HTTP_QUEUE_CAPACITY");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto str = StringHelpers::trim(env.value());
            int val = 0;
            if (NumberParserHelpers::try_parse_int(str, val)) {
                http_queue_capacity = val;
                LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
            }
        }
    }
    {
        const std::string key("HTTP_THREADS_COUNT");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto str = StringHelpers::trim(env.value());
            int val = 0;
            if (NumberParserHelpers::try_parse_int(str, val)) {
                http_threads_count = val;
                LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
            }
        }
    }

    {
        const std::string key("PROMETHEUS_PORT");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto str = StringHelpers::trim(env.value());
            int val = 0;
            if (NumberParserHelpers::try_parse_int(str, val)) {
                prometheus_port = val;
                LOGGER_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
            }
        }
    }

    auto errors = validate();
    for (auto& err : errors) {
        LOGGER_ERROR(err);
    }
}

const Configuration& Configuration::instance()
{
    static Configuration singleton(Logging::configure_logger({ {"type", "stdout"}, {"color", "true"}, {"level", "5"} }));
    return singleton;
}

void Configuration::show_configuration() const
{
    std::stringstream ss;
    ss << "configuration:";

    ss << "\n  pgsql_master.url="       << std::quoted(pgsql_master.url);
    // ss << "\n  pgsql_master.login="     << std::string(pgsql_master.login.size(), '*');
    // ss << "\n  pgsql_master.password="  << std::string(pgsql_master.password.size(), '*');
    ss << "\n  pgsql_master.login="     << pgsql_master.login;
    ss << "\n  pgsql_master.password="  << pgsql_master.password;

for (int i = 0; const auto& replica : pgsql_replica) {
    ss << "\n  pgsql_replica." << i << ".url="       << std::quoted(replica.url);
    // ss << "\n  pgsql_replica." << i << ".login="     << std::string(replica.login.size(), '*');
    // ss << "\n  pgsql_replica." << i << ".password="  << std::string(replica.password.size(), '*');
    ss << "\n  pgsql_replica." << i << ".login="     << replica.login;
    ss << "\n  pgsql_replica." << i << ".password="  << replica.password;
    ++i;
}

    ss << "\n  grpc.url="               << std::quoted(grpc_url);

    ss << "\n  kafka.url="              << std::quoted(kafka_url);

    ss << "\n  http.listening="         << std::quoted(http_listening);
    ss << "\n  http.threads_count="     << http_threads_count;
    ss << "\n  http.queue_capacity="    << http_queue_capacity;

    ss << "\n  prometheus.listening="   << prometheus_listening;

    LOGGER_DEBUG(ss.str());
}
