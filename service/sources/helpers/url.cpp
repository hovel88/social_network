#include <format>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unistd.h>
#include "helpers/url.h"
#include "helpers/string.h"
#include "helpers/number_parser.h"
#include "helpers/filepath.h"

namespace SocialNetwork {

namespace UrlHelpers {

const std::string Url::URL_RESERVED_PATH        = "?#";
const std::string Url::URL_RESERVED_QUERY       = "?#/:;+@";
const std::string Url::URL_RESERVED_QUERY_PARAM = "?#/:;+@&=";
const std::string Url::URL_RESERVED_FRAGMENT    = "";
const std::string Url::URL_ILLEGAL              = "%<>{}|\\\"^`!*'()$,[]";
const std::map<std::string, uint16_t> Url::URL_WELL_KNOWN_PORTS =
{
    // scheme   =>  port
    {"ftp",         21},
    {"ssh",         22},
    {"telnet",      23},
    {"smtp",        25},
    {"dns",         53},
    {"http",        80},
    {"ws",          80},
    {"nntp",        119},
    {"imap",        143},
    {"ldap",        389},
    {"https",       443},
    {"wss",         443},
    {"smtps",       465},
    {"rtsp",        554},
    {"ldaps",       636},
    {"dnss",        853},
    {"imaps",       993},
    {"sip",         5060},
    {"sips",        5061},
    {"xmpp",        5222},
    {"postgresql",  5432},
    {"amqp",        5672}
};

Url::Url(const std::string& scheme, const std::string& path_query_fragment)
:   scheme_(scheme)
{
    StringHelpers::to_lowercase_in_place(scheme_);
    std::string::const_iterator beg = path_query_fragment.begin();
    std::string::const_iterator end = path_query_fragment.end();
    parse_path_query_fragment(beg, end);
}

Url::Url(const std::string& scheme, const std::string& authority, const std::string& path_query_fragment)
:   scheme_(scheme)
{
    StringHelpers::to_lowercase_in_place(scheme_);
    std::string::const_iterator beg = authority.begin();
    std::string::const_iterator end = authority.end();
    parse_authority(beg, end);
    beg = path_query_fragment.begin();
    end = path_query_fragment.end();
    parse_path_query_fragment(beg, end);
}

Url::Url(const std::string& scheme, const std::string& authority, const std::string& path, const std::string& query)
:   scheme_(scheme),
    path_(path),
    query_(query)
{
    StringHelpers::to_lowercase_in_place(scheme_);
    std::string::const_iterator beg = authority.begin();
    std::string::const_iterator end = authority.end();
    parse_authority(beg, end);
}

Url::Url(const std::string& scheme, const std::string& authority, const std::string& path, const std::string& query, const std::string& fragment)
:   scheme_(scheme),
    path_(path),
    query_(query),
    fragment_(fragment)
{
    StringHelpers::to_lowercase_in_place(scheme_);
    std::string::const_iterator beg = authority.begin();
    std::string::const_iterator end = authority.end();
    parse_authority(beg, end);
}

Url::Url(const FsHelpers::FilePath& path)
:   scheme_("file")
{
    path_ = path.absolute_path().value_or("");
}

Url::Url(const char* url)
{
    parse(std::string(url));
}

Url::Url(const std::string& url)
{
    parse(url);
}

Url& Url::operator=(const char* url)
{
    clear();
    parse(std::string(url));
    return *this;
}

Url& Url::operator=(const std::string& url)
{
    clear();
    parse(url);
    return *this;
}

bool Url::empty() const
{
    return scheme_.empty()
        && host_.empty()
        && path_.empty()
        && query_.empty()
        && fragment_.empty();
}

void Url::clear()
{
    scheme_.clear();
    user_info_.clear();
    host_.clear();
    port_ = 0;
    path_.clear();
    query_.clear();
    fragment_.clear();
}

void Url::swap(Url& url) noexcept
{
    std::swap(scheme_, url.scheme_);
    std::swap(user_info_, url.user_info_);
    std::swap(host_, url.host_);
    std::swap(port_, url.port_);
    std::swap(path_, url.path_);
    std::swap(query_, url.query_);
    std::swap(fragment_, url.fragment_);
}

bool Url::operator==(const Url& url) const
{
    return equals(*this, url);
}

bool Url::operator==(const std::string& url) const
{
    Url parsed_url(url);
    return equals(*this, parsed_url);
}

bool Url::operator!=(const Url& url) const
{
    return !equals(*this, url);
}

bool Url::operator!=(const std::string& url) const
{
    Url parsed_url(url);
    return !equals(*this, parsed_url);
}

bool Url::is_relative() const
{
    return scheme_.empty();
}

void Url::normalize()
{
    if (path_.empty()) return;
    bool remove_leading = !is_relative();
    bool leading_slash  = *(path_.begin()) == '/';
    bool trailing_slash = *(path_.rbegin()) == '/';

    std::vector<std::string> segments;
    std::vector<std::string> normalized;
    get_path_segments(segments);
    for (const auto& s: segments) {
        if (s == "..") {
            if (!normalized.empty()) {
                if (normalized.back() == "..")
                    normalized.push_back(s);
                else
                    normalized.pop_back();
            } else
            if (!remove_leading) {
                normalized.push_back(s);
            }
        } else
        if (s != ".") {
            normalized.push_back(s);
        }
    }

    build_path(normalized, leading_slash, trailing_slash);
}

std::string Url::to_string() const
{
    std::string url;
    if (is_relative()) {
        encode(path_, URL_RESERVED_PATH, url);
    } else {
        url += std::format("{}:", scheme_);
        std::string auth = get_authority();
        if (!auth.empty() || scheme_ == "file") {
            url += std::format("//{}", auth);
        }
        if (!path_.empty()) {
            if (!auth.empty() && path_[0] != '/') {
                url += '/';
            }
            encode(path_, URL_RESERVED_PATH, url);
        } else
        if (!query_.empty() || !fragment_.empty()) {
            url += '/';
        }
    }
    if (!query_.empty()) {
        url += std::format("?{}", query_);
    }
    if (!fragment_.empty()) {
        url += std::format("#{}", fragment_);
    }
    return url;
}

const std::string& Url::get_scheme() const
{
    return scheme_;
}

Url& Url::set_scheme(const std::string& scheme)
{
    scheme_ = scheme;
    StringHelpers::to_lowercase_in_place(scheme_);
    return *this;
}

std::string Url::get_authority() const
{
    std::string auth;
    if (!user_info_.empty()) {
        auth += std::format("{}@", user_info_);
    }
    if (host_.find(':') != std::string::npos) {
        auth += std::format("[{}]", host_);
    } else {
        auth += host_;
    }
    if (port_) {
        auth += std::format(":{}", port_);
    } else {
        auto port = URL_WELL_KNOWN_PORTS.find(scheme_);
        if (port != URL_WELL_KNOWN_PORTS.end()) {
            auth += std::format(":{}", port->second);
        }
    }
    return auth;
}

Url& Url::set_authority(const std::string& authority)
{
    user_info_.clear();
    host_.clear();
    port_ = 0;
    std::string::const_iterator beg = authority.begin();
    std::string::const_iterator end = authority.end();
    parse_authority(beg, end);
    return *this;
}

const std::string& Url::get_user_info() const
{
    return user_info_;
}

Url& Url::set_user_info(const std::string& user_info)
{
    user_info_.clear();
    decode(user_info, user_info_);
    return *this;
}

const std::string& Url::get_host() const
{
    return host_;
}

Url& Url::set_host(const std::string& host)
{
    host_ = host;
    return *this;
}

uint16_t Url::get_specified_port() const
{
    return port_;
}

uint16_t Url::get_port() const
{
    if (port_) return port_;

    auto port = URL_WELL_KNOWN_PORTS.find(scheme_);
    if (port != URL_WELL_KNOWN_PORTS.end()) {
        return port->second;
    }

    return 0;
}

Url& Url::set_port(uint16_t port)
{
    port_ = port;
    return *this;
}

const std::string& Url::get_path() const
{
    return path_;
}

Url& Url::set_path(const std::string& path)
{
    path_.clear();
    decode(path, path_);
    return *this;
}

const std::string& Url::get_raw_query() const
{
    return query_;
}

Url& Url::set_raw_query(const std::string& query)
{
    query_ = query;
    return *this;
}

std::string Url::get_query() const
{
    std::string query;
    decode(query_, query);
    return query;
}

Url& Url::set_query(const std::string& query)
{
    query_.clear();
    encode(query, URL_RESERVED_QUERY, query_);
    return *this;
}

void Url::add_query_parameter(const std::string& param, const std::string& val)
{
    if (!query_.empty()) query_ += '&';
    encode(param, URL_RESERVED_QUERY_PARAM, query_);
    query_ += '=';
    encode(val, URL_RESERVED_QUERY_PARAM, query_);
}

Url::QueryParams_t Url::get_query_parameters(bool plus_is_space) const
{
    QueryParams_t result;
    std::string::const_iterator it(query_.begin());
    std::string::const_iterator end(query_.end());
    while (it != end) {
        std::string name;
        std::string value;
        while (it != end && *it != '=' && *it != '&') {
            if (plus_is_space && (*it == '+')) name += ' ';
            else                               name += *it;
            ++it;
        }
        if (it != end && *it == '=') {
            ++it;
            while (it != end && *it != '&') {
                if (plus_is_space && (*it == '+')) value += ' ';
                else                               value += *it;
                ++it;
            }
        }
        std::string decoded_name;
        std::string decoded_value;
        decode(name, decoded_name);
        decode(value, decoded_value);
        result.push_back(std::make_pair(decoded_name, decoded_value));
        if (it != end && *it == '&') ++it;
    }
    return result;
}

Url& Url::set_query_parameters(const QueryParams_t& params)
{
    query_.clear();
    for (const auto& p: params) {
        add_query_parameter(p.first, p.second);
    }
    return *this;
}

std::string Url::get_raw_fragment() const
{
    return fragment_;
}

Url& Url::set_raw_fragment(const std::string& fragment)
{
    fragment_ = fragment;
    return *this;
}

std::string Url::get_fragment() const
{
    std::string fragment;
    decode(fragment_, fragment);
    return fragment;
}

Url& Url::set_fragment(const std::string& fragment)
{
    fragment_.clear();
    encode(fragment, URL_RESERVED_FRAGMENT, fragment_);
    return *this;
}

void Url::get_path_segments(std::vector<std::string>& segments) const
{
    std::string::const_iterator it  = path_.begin();
    std::string::const_iterator end = path_.end();
    std::string seg;
    while (it != end) {
        if (*it == '/') {
            if (!seg.empty()) {
                segments.push_back(seg);
                seg.clear();
            }
        } else {
            seg += *it;
        }
        ++it;
    }
    if (!seg.empty()) {
        segments.push_back(seg);
    }
}

std::string Url::get_path_query() const
{
    std::string path_query;
    encode(path_, URL_RESERVED_PATH, path_query);
    if (!query_.empty()) {
        path_query += std::format("?{}", query_);
    }
    return path_query;
}

std::string Url::get_path_query_fragment() const
{
    std::string path_query_fragment;
    encode(path_, URL_RESERVED_PATH, path_query_fragment);
    if (!query_.empty()) {
        path_query_fragment += std::format("?{}", query_);
    }
    if (!fragment_.empty()) {
        path_query_fragment += std::format("#{}", fragment_);
    }
    return path_query_fragment;
}

Url& Url::set_path_query_fragment(const std::string& path_query_fragment)
{
    path_.clear();
    query_.clear();
    fragment_.clear();
    std::string::const_iterator beg = path_query_fragment.begin();
    std::string::const_iterator end = path_query_fragment.end();
    parse_path_query_fragment(beg, end);
    return *this;
}

void Url::encode(const std::string& str, const std::string& reserved, std::string& encoded)
{
    for (auto c: str) {
        if ((c >= 'a' && c <= 'z')
        ||  (c >= 'A' && c <= 'Z')
        ||  (c >= '0' && c <= '9')
        ||  (c == '-' || c == '_' || c == '.' || c == '~')) {
            encoded += c;
        } else
        if ((c <= 0x20 || c >= 0x7F)
        ||  (URL_ILLEGAL.find(c) != std::string::npos)
        ||  (reserved.find(c)    != std::string::npos)) {
            encoded += std::format("%{:02x}", c);
        } else {
            encoded += c;
        }
    }
}

void Url::decode(const std::string& str, std::string& decoded, bool plus_as_space)
{
    bool in_query = false;
    bool invalid  = false;
    std::string result;
    std::string::const_iterator it  = str.begin();
    std::string::const_iterator end = str.end();
    while (it != end) {
        char c = *it++;
        if (c == '?') {
            in_query = true;
        }
        // spaces may be encoded as plus signs in the query
        if (in_query
        &&  plus_as_space && c == '+') {
            c = ' ';
        } else
        if (c == '%') {
            if (it == end) {
                invalid = true;
                break;
            }
            char hi = *it++;
            if (it == end) {
                invalid = true;
                break;
            }
            char lo = *it++;

            char c1 = 0;
            if (hi >= '0' && hi <= '9') c1 = hi - '0';
            else
            if (hi >= 'A' && hi <= 'F') c1 = hi - 'A' + 10;
            else
            if (hi >= 'a' && hi <= 'f') c1 = hi - 'a' + 10;
            else {
                invalid = true;
                break;
            }

            char c2 = 0;
            if (lo >= '0' && lo <= '9') c2 = lo - '0';
            else
            if (lo >= 'A' && lo <= 'F') c2 = lo - 'A' + 10;
            else
            if (lo >= 'a' && lo <= 'f') c2 = lo - 'a' + 10;
            else {
                invalid = true;
                break;
            }

            c = static_cast<char>(16*c1) + c2;
        }
        result += c;
    }
    if (!invalid) decoded = result;
}

bool Url::equals(const Url& u1, const Url& u2)  
{
    return u1.scheme_     == u2.scheme_
        && u1.user_info_  == u2.user_info_
        && u1.host_       == u2.host_
        && u1.get_port()  == u2.get_port()
        && u1.path_       == u2.path_
        && u1.query_      == u2.query_
        && u1.fragment_   == u2.fragment_;
}

void Url::parse(const std::string& url)
{
    std::string::const_iterator it  = url.begin();
    std::string::const_iterator end = url.end();
    if (it == end) return;

    if (*it != '/' && *it != '.' && *it != '?' && *it != '#') {
        std::string scheme;
        while (it != end && *it != ':' && *it != '?' && *it != '#' && *it != '/') {
            scheme += *it++;
        }

        if (it != end && *it == ':') {
            ++it;
            if (it == end) {
                throw std::runtime_error(std::format("URL scheme must be followed by authority or path ({})", url));
            }
            set_scheme(scheme);
            if (*it == '/') {
                ++it;
                if (it != end && *it == '/') {
                    ++it;
                    parse_authority(it, end);
                } else {
                    --it;
                }
            }
            parse_path_query_fragment(it, end);
        } else {
            it = url.begin();
            parse_path_query_fragment(it, end);
        }
    } else {
        parse_path_query_fragment(it, end);
    }
}

void Url::parse_authority(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    std::string user_info;
    std::string part;
    while (it != end && *it != '/' && *it != '?' && *it != '#') {
        if (*it == '@') {
            user_info = part;
            part.clear();
        } else {
            part += *it;
        }
        ++it;
    }
    std::string::const_iterator pbeg = part.begin();
    std::string::const_iterator pend = part.end();
    parse_host_and_port(pbeg, pend);

    user_info_ = user_info;
}

void Url::parse_host_and_port(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    if (it == end) return;

    std::string host;
    if (*it == '[') {
        // IPv6 address
        ++it;
        while (it != end && *it != ']') {
            host += *it++;
        }
        if (it == end) {
            throw std::runtime_error(std::format("unterminated IPv6 address"));
        }
        ++it;
    } else {
        // IPv4 address
        while (it != end && *it != ':') {
            host += *it++;
        }
    }

    if (it != end && *it == ':') {
        ++it;
        std::string port;
        while (it != end) {
            port += *it++;
        }
        if (!port.empty()) {
            int nport = 0;
            if (NumberParserHelpers::try_parse_int(port, nport)
            &&  (nport > 0 && nport < 65536)) {
                port_ = static_cast<uint16_t>(nport);
            } else {
                throw std::runtime_error(std::format("bad or invalid port number ({})", port));
            }
        } else {
            port_ = 0;
        }
    } else {
        port_ = 0;
    }

    host_ = host;
    if (host_.size() && host_[0] != '%') {
        StringHelpers::to_lowercase_in_place(host_);
    }
}

void Url::parse_path_query_fragment(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    if (it != end && *it != '?' && *it != '#') {
        parse_path(it, end);
    }

    if (it != end && *it == '?') {
        ++it;
        parse_query(it, end);
    }

    if (it != end && *it == '#') {
        ++it;
        parse_fragment(it, end);
    }
}

void Url::parse_path(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    std::string path;
    while (it != end && *it != '?' && *it != '#') {
        path += *it++;
    }
    decode(path, path_);
}

void Url::parse_query(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    query_.clear();
    while (it != end && *it != '#') {
        query_ += *it++;
    }
}

void Url::parse_fragment(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    fragment_.clear();
    while (it != end) {
        fragment_ += *it++;
    }
}

void Url::build_path(const std::vector<std::string>& segments, bool leading_slash, bool trailing_slash)
{
    path_.clear();
    bool first = true;
    for (const auto& s: segments) {
        if (first) {
            first = false;
            if (leading_slash) {
                path_ += '/';
            } else
            if (scheme_.empty()
            &&  s.find(':') != std::string::npos) {
                path_.append("./");
            }
        } else {
            path_ += '/';
        }
        path_.append(s);
    }
    if (trailing_slash) {
        path_ += '/';
    }
}

} // namespace UrlHelpers

} // namespace SocialNetwork
