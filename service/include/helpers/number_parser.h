#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace SocialNetwork {

namespace NumberParserHelpers {

std::optional<bool> parse_bool(const std::string& s);
bool try_parse_bool(const std::string& s, bool& value);

std::optional<double> parse_float(const std::string& s);
bool try_parse_float(const std::string& s, double& value);

std::optional<uint32_t> parse_hex(const std::string& s);
std::optional<uint64_t> parse_hex64(const std::string& s);
bool try_parse_hex(const std::string& s, uint32_t& value);
bool try_parse_hex64(const std::string& s, uint64_t& value);

std::optional<uint32_t> parse_oct(const std::string& s);
std::optional<uint64_t> parse_oct64(const std::string& s);
bool try_parse_oct(const std::string& s, uint32_t& value);
bool try_parse_oct64(const std::string& s, uint64_t& value);

std::optional<uint32_t> parse_uint(const std::string& s);
std::optional<uint64_t> parse_uint64(const std::string& s);
bool try_parse_uint(const std::string& s, uint32_t& value);
bool try_parse_uint64(const std::string& s, uint64_t& value);

std::optional<int> parse_int(const std::string& s);
std::optional<int64_t> parse_int64(const std::string& s);
bool try_parse_int(const std::string& s, int& value);
bool try_parse_int64(const std::string& s, int64_t& value);

} // namespace NumberParserHelpers

} // namespace SocialNetwork
