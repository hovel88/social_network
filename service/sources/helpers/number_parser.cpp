#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <limits>
#include <string>
#include <algorithm>
#include <stdexcept>
#include "helpers/number_parser.h"

namespace SocialNetwork {

namespace NumberParserHelpers {

std::optional<bool> parse_bool(const std::string& s)
{
    bool result{false};
    if (try_parse_bool(s, result)) return result;
    return std::nullopt;
}

bool try_parse_bool(const std::string& s, bool& value)
{
    int n = 0;
    if (try_parse_int(s, n)) {
        value = (n != 0);
        return true;
    }

    std::string str(s);
    std::transform(str.cbegin(), str.cend(), str.begin(),
        [](unsigned char ch){ return std::tolower(ch); });

    if (str.compare("true") == 0) {
        value = true;
        return true;
    } else
    if (str.compare("yes") == 0) {
        value = true;
        return true;
    } else
    if (str.compare("on") == 0) {
        value = true;
        return true;
    }

    if (str.compare("false") == 0) {
        value = false;
        return true;
    } else
    if (str.compare("no") == 0) {
        value = false;
        return true;
    } else
    if (str.compare("off") == 0) {
        value = false;
        return true;
    }

    return false;
}

std::optional<double> parse_float(const std::string& s)
{
    double result{0.0};
    if (try_parse_float(s, result)) return result;
    return std::nullopt;
}

bool try_parse_float(const std::string& s, double& value)
{
    try {
        double v{std::stod(s)};
        value = v;
    // } catch (std::invalid_argument& ex) {
    //     std::cerr << "Invalid argument: " << ex.what() << std::endl;
    //     return false;
    // } catch (std::out_of_range& ex) {
    //     std::cerr << "Out of range: " << ex.what() << std::endl;
    //     return false;
    } catch (...) {
        return false;
    }
    return true;
}


std::optional<uint32_t> parse_hex(const std::string& s)
{
    uint32_t result{0};
    if (try_parse_hex(s, result)) return result;
    return std::nullopt;
}

std::optional<uint64_t> parse_hex64(const std::string& s)
{
    uint64_t result{0};
    if (try_parse_hex64(s, result)) return result;
    return std::nullopt;
}

bool try_parse_hex(const std::string& s, uint32_t& value)
{
    try {
        auto v{std::stoul(s, nullptr, 0x10)};
        value = static_cast<uint32_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

bool try_parse_hex64(const std::string& s, uint64_t& value)
{
    try {
        auto v{std::stoull(s, nullptr, 0x10)};
        value = static_cast<uint64_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

std::optional<uint32_t> parse_oct(const std::string& s)
{
    uint32_t result{0};
    if (try_parse_oct(s, result)) return result;
    return std::nullopt;
}

std::optional<uint64_t> parse_oct64(const std::string& s)
{
    uint64_t result{0};
    if (try_parse_oct64(s, result)) return result;
    return std::nullopt;
}

bool try_parse_oct(const std::string& s, uint32_t& value)
{
    try {
        auto v{std::stoul(s, nullptr, 010)};
        value = static_cast<uint32_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

bool try_parse_oct64(const std::string& s, uint64_t& value)
{
    try {
        auto v{std::stoull(s, nullptr, 010)};
        value = static_cast<uint64_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

std::optional<uint32_t> parse_uint(const std::string& s)
{
    uint32_t result{0};
    if (try_parse_uint(s, result)) return result;
    return std::nullopt;
}

std::optional<uint64_t> parse_uint64(const std::string& s)
{
    uint64_t result{0};
    if (try_parse_uint64(s, result)) return result;
    return std::nullopt;
}

bool try_parse_uint(const std::string& s, uint32_t& value)
{
    try {
        auto v{std::stoul(s, nullptr, 10)};
        value = static_cast<uint32_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

bool try_parse_uint64(const std::string& s, uint64_t& value)
{
    try {
        auto v{std::stoull(s, nullptr, 10)};
        value = static_cast<uint64_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

std::optional<int> parse_int(const std::string& s)
{
    int result{0};
    if (try_parse_int(s, result)) return result;
    return std::nullopt;
}

std::optional<int64_t> parse_int64(const std::string& s)
{
    int64_t result{0};
    if (try_parse_int64(s, result)) return result;
    return std::nullopt;
}

bool try_parse_int(const std::string& s, int& value)
{
    try {
        auto v{std::stoi(s, nullptr, 10)};
        value = static_cast<int>(v);
    } catch (...) {
        return false;
    }
    return true;
}

bool try_parse_int64(const std::string& s, int64_t& value)
{
    try {
        auto v{std::stoll(s, nullptr, 10)};
        value = static_cast<int64_t>(v);
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace NumberParserHelpers

} // namespace SocialNetwork
