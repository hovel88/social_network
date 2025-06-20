#include "logger/logger_base.h"
#include "logger/logger_factory.h"

namespace SocialNetwork {

void Logging::Logger::log(const std::string_view /*message*/, const LogLevel /*level*/)
{
}

void Logging::Logger::log(const std::string_view /*message*/, const std::string_view /*custom_directive*/)
{
}

std::string Logging::Logger::get_timestamp()
{
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm gmt{};
    get_gmtime(&tt, &gmt);

    std::chrono::duration<double> fractional_seconds = (tp - std::chrono::system_clock::from_time_t(tt)) + std::chrono::seconds(gmt.tm_sec);

    std::string buffer("yyyy-mm-dd hh:mm:ss.xxxxxx0");
    [[maybe_unused]] int ret = snprintf(&buffer.front(), buffer.length(), "%04d-%02d-%02d %02d:%02d:%09.6f",
                                        (gmt.tm_year + 1900) %10000u,
                                        (gmt.tm_mon + 1)     %100u,
                                        (gmt.tm_mday)        %100u,
                                        (gmt.tm_hour)        %100u,
                                        (gmt.tm_min)         %100u,
                                        fractional_seconds.count());

    // remove trailing null terminator added by snprintf.
    buffer.pop_back();
    return buffer;
}

std::tm* Logging::Logger::get_gmtime(const std::time_t* time, std::tm* tm)
{
    return gmtime_r(time, tm);
}

namespace Logging {

bool null_logger_registered = register_logger("null", [](const LoggerConfig& config)->std::shared_ptr<Logger>
{
    return std::make_shared<Logger>(config);
});

} // namespace Logging

} // namespace SocialNetwork
