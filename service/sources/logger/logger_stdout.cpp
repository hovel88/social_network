#include <stdexcept>
#include "logger/logger_stdout.h"
#include "logger/logger_factory.h"

namespace SocialNetwork {

void Logging::LoggerStdOut::log(const std::string_view message, const LogLevel level)
{
    log(message, levels_.find(level)->second);
}

void Logging::LoggerStdOut::log(const std::string_view message, const std::string_view custom_directive)
{
    std::string output;
    output.reserve(message.length() + 64);
    output.append(Logger::get_timestamp());
    output.append(custom_directive);
    output.append(message);
    output.push_back('\n');
    // ВАЖНО!!!
    //      cout is thread safe, to avoid multiple threads interleaving on one line
    //      though, we make sure to only call the << operator once on std::cout
    //      otherwise the << operators from different threads could interleave
    //      obviously we dont care if flushes interleave
    std::cout << output;
    std::cout.flush();
}

namespace Logging {

bool stdout_logger_registered = register_logger("stdout", [](const LoggerConfig& config)->std::shared_ptr<Logger>
{
    return std::make_shared<LoggerStdOut>(config);
});

} // namespace Logging

} // namespace SocialNetwork
