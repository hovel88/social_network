#include "logger/logger_factory.h"
#include "logger/logger_base.h"

namespace SocialNetwork {

std::shared_ptr<Logging::Logger> Logging::LoggerFactory::produce(const LoggerConfig& config) const
{
    auto type = config.find("type");
    if (type == config.end()) {
        throw std::runtime_error("logging factory configuration requires a type of logger");
    }

    auto found = find(type->second);
    if (found == end()) {
        throw std::runtime_error("couldn't produce logger for type: " + type->second);
    }

    return found->second(config);
}

// --------------------------------------------------------

Logging::LoggerFactory& Logging::get_single_factory()
{
    static LoggerFactory factory_singleton{};
    return factory_singleton;
}

bool Logging::register_logger(const std::string_view name, LoggerCreator function_ptr)
{
    return get_single_factory().emplace(name, function_ptr).second;
}

} // namespace SocialNetwork
