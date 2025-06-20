#include <format>
#include "helpers/environment.h"
#include "helpers/string.h"
#include "helpers/number_parser.h"
#include "configuration/configuration.h"

namespace SocialNetwork {

const config_data::config_s& Configuration::config() const noexcept
{
    return current_configuration_;
}

void Configuration::show_configuration() const
{
    std::stringstream ss;
    ss << "configuration:";
    ss << "\n  pgsql.endpoint="         << std::quoted(current_configuration_.pgsql_endpoint);
    // ss << "\n  pgsql.login="            << std::string(current_configuration_.pgsql_login.size(), '*');
    // ss << "\n  pgsql.password="         << std::string(current_configuration_.pgsql_password.size(), '*');
    ss << "\n  pgsql.login="            << current_configuration_.pgsql_login;
    ss << "\n  pgsql.password="         << current_configuration_.pgsql_password;
    ss << "\n  http.listening="         << std::quoted(current_configuration_.http_listening);
    ss << "\n  http.threads_count="     << current_configuration_.http_threads_count;
    ss << "\n  http.queue_capacity="    << current_configuration_.http_queue_capacity;
    LOG_DEBUG(ss.str());
}

void Configuration::apply_()
{
    current_configuration_.init();
    LOG_INFOR(std::format("configuration was initialized with default values"));

    {
        const std::string key("PGSQL_ENDPOINT");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_endpoint = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_LOGIN");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_login = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_PASSWORD");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_password = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }

    {
        const std::string key("HTTP_LISTENING");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.http_listening = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("HTTP_QUEUE_CAPACITY");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto str = StringHelpers::trim(env.value());
            int val = 0;
            if (NumberParserHelpers::try_parse_int(str, val)) {
                current_configuration_.http_queue_capacity = val;
                LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
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
                current_configuration_.http_threads_count = val;
                LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
            }
        }
    }

    auto errors = current_configuration_.validate();
    for (auto& err : errors) {
        LOG_ERROR(err);
    }
}

} // namespace SocialNetwork
