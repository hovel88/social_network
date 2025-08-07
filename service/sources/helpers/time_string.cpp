#include "helpers/time_string.h"

namespace TimeStringHelpers {

std::tm localtime(const time_t& t)
{
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm;
}

std::tm utctime(const time_t& t)
{
    std::tm tm{};
    gmtime_r(&t, &tm);
    return tm;
}

std::string to_datatime_format(const time_t& t, bool utc)
{
    char str[std::size("2025-04-09 11:53:35")];
    std::strftime(std::data(str), std::size(str), "%F %T", (utc ? std::gmtime(&t) : std::localtime(&t)));
    return std::string(str);
}

std::string to_http_gmt_format(const time_t& t)
{
    char str[std::size("Sat, 01 Jan 2005 11:00:00 GMT")];
    std::strftime(std::data(str), std::size(str), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&t));
    return std::string(str);
}

std::string to_iso8601z_format(const time_t& t)
{
    char str[std::size("2025-07-09T17:49:00Z")];
    std::strftime(std::data(str), std::size(str), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return std::string(str);
}

} // namespace TimeStringHelpers
