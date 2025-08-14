#pragma once

#include <string>
#include <chrono>
#include <ctime>

namespace TimeStringHelpers {

std::tm localtime(const time_t& t);
std::tm utctime(const time_t& t);

std::string to_datatime_format(const time_t& t, bool utc = false); // "2025-04-09 11:53:35"
std::string to_http_gmt_format(const time_t& t);                   // "Sat, 01 Jan 2005 11:00:00 GMT"
std::string to_iso8601z_format(const time_t& t);                   // "2025-07-09T17:49:00Z"

static inline time_t   localtime_seconds()  { return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); }
static inline uint64_t localtime_millisec() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
static inline uint64_t monotonic_millisec() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

} // namespace TimeStringHelpers
