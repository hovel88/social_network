#include <format>
#include <filesystem>
#include "helpers/environment.h"
#include "helpers/string.h"
#include "helpers/number_parser.h"
#include "configuration/configuration.h"

namespace SocialNetwork {

std::shared_ptr<cxxopts::ParseResult> configure_cli_options(int argc, char** argv)
{
    auto prog_name = std::filesystem::path(std::string(argv[0])).filename().string();

    try {
        cxxopts::Options options(prog_name, "Social network service");

        options.allow_unrecognised_options();
        options.set_width(80);
        options.add_options()
        ("h,help",         "Display usage help and exit")
        ("pgsql_endpoint", "Endpoint URL to PostgreSQL server", cxxopts::value<std::string>())
        ("pgsql_login",    "Specify client login to authorize on PostgreSQL server", cxxopts::value<std::string>())
        ("pgsql_password", "Specify client password to authorize on PostgreSQL server", cxxopts::value<std::string>())
        ("http_listening", "Address and port (ip:port) HTTP server starts listening on", cxxopts::value<std::string>())
        ("http_queue",     "Max available requests queue capacity for HTTP server", cxxopts::value<int>())
        ("http_threads",   "Max available threads count to handle HTTP requests", cxxopts::value<int>())
        // TODO: добавлять сюда ключи для создания/удаления индексов в БД
        ;

        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }
        if (!result.unmatched().empty()) {
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < result.unmatched().size(); ++i) {
                ss << result.unmatched().at(i);
                ss << (i + 1 == result.unmatched().size() ? "" : ",");
            }
            ss << "]\n";
            std::cout << "unrecognised options: " << ss.str() << std::endl;
            std::cout << options.help() << std::endl;
            exit(0);
        }

        return std::make_shared<cxxopts::ParseResult>(result);
    }
    catch (std::exception& ex) {
        std::cout << ex.what() << std::endl;
        exit(0);
    }
    return nullptr;
}

// ----------------------------------------------------------------------------

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

void Configuration::apply_(std::shared_ptr<cxxopts::ParseResult> cli_opts)
{
    current_configuration_.init();
    LOG_INFOR(std::format("configuration was initialized with default values"));

    LOG_INFOR(std::format("trying to read configuration from environment"));

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

    LOG_INFOR(std::format("trying to read configuration from command line options"));
    const auto& cli = *cli_opts;

    try {
        const std::string key("pgsql_endpoint");
        if (cli.count(key)) {
            auto val = cli[key].as<std::string>();
            current_configuration_.pgsql_endpoint = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}

    try {
        const std::string key("pgsql_login");
        if (cli.count(key)) {
            auto val = cli[key].as<std::string>();
            current_configuration_.pgsql_login = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}

    try {
        const std::string key("pgsql_password");
        if (cli.count(key)) {
            auto val = cli[key].as<std::string>();
            current_configuration_.pgsql_password = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}

    try {
        const std::string key("http_listening");
        if (cli.count(key)) {
            auto val = cli[key].as<std::string>();
            current_configuration_.http_listening = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}
    try {
        const std::string key("http_queue");
        if (cli.count(key)) {
            auto val = cli[key].as<int>();
            current_configuration_.http_queue_capacity = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}
    try {
        const std::string key("http_threads");
        if (cli.count(key)) {
            auto val = cli[key].as<int>();
            current_configuration_.http_threads_count = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}


    auto errors = current_configuration_.validate();
    for (auto& err : errors) {
        LOG_ERROR(err);
    }
}

} // namespace SocialNetwork
