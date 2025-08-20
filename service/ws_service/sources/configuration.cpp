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

    ss << "\n  kafka.url="              << std::quoted(kafka_url);

    ss << "\n  http.listening="         << std::quoted(http_listening);
    ss << "\n  http.threads_count="     << http_threads_count;
    ss << "\n  http.queue_capacity="    << http_queue_capacity;

    ss << "\n  ws.listening="           << std::quoted(http_listening);

    ss << "\n  prometheus.listening="   << prometheus_listening;

    LOGGER_DEBUG(ss.str());
}
