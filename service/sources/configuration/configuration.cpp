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
        ("h,help",              "Display usage help and exit")
        ("pgsql_master_url",    "Endpoint URL (with login:password to authorize) to PostgreSQL master server", cxxopts::value<std::string>())
        ("pgsql_replica_url",   "Endpoint URL (with login:password to authorize) to PostgreSQL replica server", cxxopts::value<std::vector<std::string>>())
        ("http_listening",      "Address and port (ip:port) HTTP server starts listening on", cxxopts::value<std::string>())
        ("http_queue",          "Max available requests queue capacity for HTTP server", cxxopts::value<int>())
        ("http_threads",        "Max available threads count to handle HTTP requests", cxxopts::value<int>())
        ("prometheus_port",     "Port Prometheus server starts listening on", cxxopts::value<int>())
        ("i,index_add",         "Add indexes into DB (names_search) ", cxxopts::value<std::vector<std::string>>())
        ("I,index_drop",        "Drop indexes into DB (names_search) ", cxxopts::value<std::vector<std::string>>())
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
    ss << "\n  pgsql_master.url="       << std::quoted(current_configuration_.pgsql_master.url);
    // ss << "\n  pgsql_master.login="            << std::string(current_configuration_.pgsql_master.login.size(), '*');
    // ss << "\n  pgsql_master.password="         << std::string(current_configuration_.pgsql_master.password.size(), '*');
    ss << "\n  pgsql_master.login="     << current_configuration_.pgsql_master.login;
    ss << "\n  pgsql_master.password="  << current_configuration_.pgsql_master.password;
for (int i = 0; const auto& replica : current_configuration_.pgsql_replica) {
    ss << "\n  pgsql_replica." << i << ".url="       << std::quoted(replica.url);
    // ss << "\n  pgsql_replica." << i << ".login="            << std::string(replica.login.size(), '*');
    // ss << "\n  pgsql_replica." << i << ".password="         << std::string(replica.password.size(), '*');
    ss << "\n  pgsql_replica." << i << ".login="     << replica.login;
    ss << "\n  pgsql_replica." << i << ".password="  << replica.password;
    ++i;
}
    ss << "\n  http.listening="         << std::quoted(current_configuration_.http_listening);
    ss << "\n  http.threads_count="     << current_configuration_.http_threads_count;
    ss << "\n  http.queue_capacity="    << current_configuration_.http_queue_capacity;
    ss << "\n  prometheus.listening="   << current_configuration_.prometheus_listening;
    LOG_DEBUG(ss.str());
}

void Configuration::apply_(std::shared_ptr<cxxopts::ParseResult> cli_opts)
{
    current_configuration_.init();
    LOG_INFOR(std::format("configuration was initialized with default values"));

    LOG_INFOR(std::format("trying to read configuration from environment"));

    {
        const std::string key("PGSQL_MASTER_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_master.url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_1_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_replica.push_back({});
            current_configuration_.pgsql_replica.back().url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_2_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_replica.push_back({});
            current_configuration_.pgsql_replica.back().url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_3_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_replica.push_back({});
            current_configuration_.pgsql_replica.back().url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_4_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_replica.push_back({});
            current_configuration_.pgsql_replica.back().url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
        }
    }
    {
        const std::string key("PGSQL_REPLICA_5_URL");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto val = StringHelpers::trim(env.value());
            current_configuration_.pgsql_replica.push_back({});
            current_configuration_.pgsql_replica.back().url = val;
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

    {
        const std::string key("PROMETHEUS_PORT");
        if (EnvironmentHelpers::has(key)) {
            auto env = EnvironmentHelpers::get(key);
            auto str = StringHelpers::trim(env.value());
            int val = 0;
            if (NumberParserHelpers::try_parse_int(str, val)) {
                current_configuration_.prometheus_port = val;
                LOG_DEBUG(std::format("configuration parameter was replaced by environment variable '{}'", key));
            }
        }
    }

    LOG_INFOR(std::format("trying to read configuration from command line options"));
    const auto& cli = *cli_opts;

    try {
        const std::string key("pgsql_master_url");
        if (cli.count(key)) {
            auto val = cli[key].as<std::string>();
            current_configuration_.pgsql_master.url = val;
            LOG_DEBUG(std::format("configuration parameter was replaced by command line option '{}'", key));
        }
    }
    catch (...) {}

    try {
        const std::string key("pgsql_replica_url");
        if (cli.count(key)) {
            auto val = cli[key].as<std::vector<std::string>>();
            for (const auto& v : val) {
                current_configuration_.pgsql_replica.push_back({});
                current_configuration_.pgsql_replica.back().url = v;
            }
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

    try {
        const std::string key("prometheus_port");
        if (cli.count(key)) {
            auto val = cli[key].as<int>();
            current_configuration_.prometheus_port = val;
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
