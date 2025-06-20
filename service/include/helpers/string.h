#pragma once

#include <string>

namespace SocialNetwork {

namespace StringHelpers {

std::string trim_left(const std::string& s);
std::string trim_right(const std::string& s);
std::string trim(const std::string& s);

std::string& trim_left_in_place(std::string& s);
std::string& trim_right_in_place(std::string& s);
std::string& trim_in_place(std::string& s);

std::string replace(const std::string& s, const std::string& from, const std::string& to, size_t start = 0);
std::string replace(const std::string& s, const char from, const char to, size_t start = 0);
std::string remove(const std::string& s, const char ch, size_t start = 0);
std::string translate(const std::string& s, const std::string& from, const std::string& to);

std::string& replace_in_place(std::string& s, const std::string& from, const std::string& to, size_t start = 0);
std::string& replace_in_place(std::string& s, const char from, const char to, size_t start = 0);
std::string& remove_in_place(std::string& s, const char ch, size_t start = 0);
std::string& translate_in_place(std::string& s, const std::string& from, const std::string& to);

bool is_ascii_ch(int ch);
char to_uppercase_ch(unsigned char ch);
char to_lowercase_ch(unsigned char ch);

std::string to_uppercase(const std::string& s);
std::string to_lowercase(const std::string& s);

std::string& to_uppercase_in_place(std::string& s);
std::string& to_lowercase_in_place(std::string& s);

} // namespace StringHelpers

} // namespace SocialNetwork
