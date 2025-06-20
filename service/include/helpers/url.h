#pragma once

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <stdint.h>

namespace SocialNetwork {

namespace FsHelpers {

class FilePath;

}

namespace UrlHelpers {

class Url
{
public:
    using QueryParams_t = std::vector< std::pair<std::string, std::string> >;

public:
    ~Url() = default;
    Url() = default;
    Url(const Url&) = default;
    Url(Url&&) noexcept = default;
    Url& operator=(const Url&) = default;
    Url& operator=(Url&&) noexcept = default;

    Url(const std::string& scheme, const std::string& path_query_fragment);
    Url(const std::string& scheme, const std::string& authority, const std::string& path_query_fragment);
    Url(const std::string& scheme, const std::string& authority, const std::string& path, const std::string& query);
    Url(const std::string& scheme, const std::string& authority, const std::string& path, const std::string& query, const std::string& fragment);
    explicit Url(const FsHelpers::FilePath& path);
    explicit Url(const char* url);
    explicit Url(const std::string& url);
    Url& operator=(const char* url);
    Url& operator=(const std::string& url);

    bool empty() const;
    void clear();
    void swap(Url& url) noexcept;

    bool operator==(const Url& url) const;
    bool operator==(const std::string& url) const;
    bool operator!=(const Url& url) const;
    bool operator!=(const std::string& url) const;

    bool is_relative() const;
    void normalize(); // removing all leading . and .. segments from the path

    std::string to_string() const;

    const std::string& get_scheme() const;
    Url& set_scheme(const std::string& scheme);

    std::string get_authority() const; // (userInfo, host, port)
    Url& set_authority(const std::string& authority);

    const std::string& get_user_info() const;
    Url& set_user_info(const std::string& user_info);

    const std::string& get_host() const;
    Url& set_host(const std::string& host);

    uint16_t get_specified_port() const;
    uint16_t get_port() const;
    Url& set_port(uint16_t port);

    const std::string& get_path() const;
    Url& set_path(const std::string& path);

    const std::string& get_raw_query() const;     // usually means percent encoded string
    Url& set_raw_query(const std::string& query); // string must be properly percent-encoded

    std::string get_query() const;            // returns the decoded query
    Url& set_query(const std::string& query); // query will be percent-encoded

    void add_query_parameter(const std::string& param, const std::string& val = ""); // adds "param=val" to the query
    QueryParams_t get_query_parameters(bool plus_is_space = true) const;
    Url& set_query_parameters(const QueryParams_t& params);

    std::string get_raw_fragment() const;               // usually means percent encoded string
    Url& set_raw_fragment(const std::string& fragment); // fragment must be properly percent-encoded

    std::string get_fragment() const;
    Url& set_fragment(const std::string& fragment);

    void get_path_segments(std::vector<std::string>& segments) const;
    std::string get_path_query() const;
    std::string get_path_query_fragment() const;
    Url& set_path_query_fragment(const std::string& path_query_fragment);

    static void encode(const std::string& str, const std::string& reserved, std::string& encoded);
    static void decode(const std::string& str, std::string& decoded, bool plus_as_space = false);

protected:
    static const std::string URL_RESERVED_PATH;
    static const std::string URL_RESERVED_QUERY;
    static const std::string URL_RESERVED_QUERY_PARAM;
    static const std::string URL_RESERVED_FRAGMENT;
    static const std::string URL_ILLEGAL;
    static const std::map<std::string, uint16_t> URL_WELL_KNOWN_PORTS;

private:
    std::string scheme_{};
    std::string user_info_{};
    std::string host_{};
    uint16_t    port_{0};
    std::string path_{};
    std::string query_{};
    std::string fragment_{};

    static bool equals(const Url& u1, const Url& u2);

    void parse(const std::string& url);
    void parse_authority(std::string::const_iterator& it, const std::string::const_iterator& end);
    void parse_host_and_port(std::string::const_iterator& it, const std::string::const_iterator& end);
    void parse_path_query_fragment(std::string::const_iterator& it, const std::string::const_iterator& end);
    void parse_path(std::string::const_iterator& it, const std::string::const_iterator& end);
    void parse_query(std::string::const_iterator& it, const std::string::const_iterator& end);
    void parse_fragment(std::string::const_iterator& it, const std::string::const_iterator& end);

    void build_path(const std::vector<std::string>& segments, bool leading_slash, bool trailing_slash);
};

inline void swap(Url& u1, Url& u2) noexcept
{
    u1.swap(u2);
}

} // namespace UrlHelpers

} // namespace SocialNetwork
