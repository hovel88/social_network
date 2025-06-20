#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include "helpers/string.h"

namespace SocialNetwork {

namespace StringHelpers {

std::string trim_left(const std::string& s)
{
    std::string::const_iterator it  = s.begin();
    std::string::const_iterator end = s.end();

    while (it != end && std::isspace(*it) != 0) ++it;
    return std::string(it, end);
}

std::string trim_right(const std::string& s)
{
    std::ptrdiff_t pos = static_cast<std::ptrdiff_t>(s.size()) - 1;

    while (pos >= 0 && std::isspace(s[pos]) != 0) --pos;
    return std::string(s, 0, pos + 1);
}

std::string trim(const std::string& s)
{
    std::ptrdiff_t first = 0;
    std::ptrdiff_t last  = static_cast<std::ptrdiff_t>(s.size()) - 1;

    while (first <= last && std::isspace(s[first]) != 0) ++first;
    while (last >= first && std::isspace(s[last]) != 0) --last;
    return std::string(s, first, last - first + 1);
}

std::string& trim_left_in_place(std::string& s)
{
    std::string::iterator it  = s.begin();
    std::string::iterator end = s.end();

    while (it != end && std::isspace(*it) != 0) ++it;
    s.erase(s.begin(), it);
    return s;
}

std::string& trim_right_in_place(std::string& s)
{
    std::ptrdiff_t pos = static_cast<std::ptrdiff_t>(s.size()) - 1;

    while (pos >= 0 && std::isspace(s[pos]) != 0) --pos;
    s.resize(pos + 1);
    return s;
}

std::string& trim_in_place(std::string& s)
{
    std::ptrdiff_t first = 0;
    std::ptrdiff_t last  = static_cast<std::ptrdiff_t>(s.size()) - 1;

    while (first <= last && std::isspace(s[first]) != 0) ++first;
    while (last >= first && std::isspace(s[last]) != 0) --last;
    if (last >= 0) {
        s.resize(last + 1);
        s.erase(0, first);
    }
    return s;
}

std::string replace(const std::string& s, const std::string& from, const std::string& to, size_t start)
{
    std::string result(s);
    replace_in_place(result, from, to, start);
    return result;
}

std::string replace(const std::string& s, const char from, const char to, size_t start)
{
    std::string result(s);
    replace_in_place(result, from, to, start);
    return result;
}

std::string remove(const std::string& s, const char ch, size_t start)
{
    std::string result(s);
    replace_in_place(result, ch, 0, start);
    return result;
}

std::string translate(const std::string& s, const std::string& from, const std::string& to)
{
    // возвращает копию исходной строки, заменяя символы в ней.
    // ищет очередной символ в работе в from, и заменяет его
    // на символ из набора to ПО ПОЗИЦИИ. если не находит такой
    // позиции в наборе to, то из итоговой строки символ будет удален

    std::string result;
    result.reserve(s.size());
    std::string::const_iterator it  = s.begin();
    std::string::const_iterator end = s.end();
    size_t to_sz = to.size();
    while (it != end) {
        size_t pos = from.find(*it);
        if (pos == std::string::npos) {
            result += *it;
        } else {
            if (pos < to_sz) result += to[pos];
        }
        ++it;
    }
    return result;
}

std::string& replace_in_place(std::string& s, const std::string& from, const std::string& to, size_t start)
{
    if (from.empty()) return s;
    if (from == to) return s;

    std::string result;
    result.append(s, 0, start);

    size_t pos = 0;
    do {
        pos = s.find(from, start);
        if (pos != std::string::npos) {
            result.append(s, start, pos - start);
            result.append(to);
            start = pos + from.length();
        } else {
            result.append(s, start, s.size() - start);
        }
    } while (pos != std::string::npos);

    s.swap(result);
    return s;
}

std::string& replace_in_place(std::string& s, const char from, const char to, size_t start)
{
    if (from == to) return s;

    size_t pos = 0;
    do {
        pos = s.find(from, start);
        if (pos != std::string::npos) {
            if (to) s[pos] = to;
            else    s.erase(pos, 1);
        }
    } while (pos != std::string::npos);

    return s;
}

std::string& remove_in_place(std::string& s, const char ch, size_t start)
{
    return replace_in_place(s, ch, 0, start);
}

std::string& translate_in_place(std::string& s, const std::string& from, const std::string& to)
{
    s = translate(s, from, to);
    return s;
}

bool is_ascii_ch(int ch)
{
    // the ASCII range (0 .. 127)
    return (static_cast<uint32_t>(ch) & 0xFFFFFF80) == 0;
}

char to_uppercase_ch(unsigned char ch)
{
    return static_cast<char>(std::toupper(ch));
}

char to_lowercase_ch(unsigned char ch)
{
    return static_cast<char>(std::tolower(ch));
}

std::string to_uppercase(const std::string& s)
{
    std::string r(s);
    std::transform(r.cbegin(), r.cend(), r.begin(), to_uppercase_ch);
    return r;
}

std::string to_lowercase(const std::string& s)
{
    std::string r(s);
    std::transform(r.cbegin(), r.cend(), r.begin(), to_lowercase_ch);
    return r;
}

std::string& to_uppercase_in_place(std::string& s)
{
    std::transform(s.cbegin(), s.cend(), s.begin(), to_uppercase_ch);
    return s;
}

std::string& to_lowercase_in_place(std::string& s)
{
    std::transform(s.cbegin(), s.cend(), s.begin(), to_lowercase_ch);
    return s;
}

} // namespace StringHelpers

} // namespace SocialNetwork
