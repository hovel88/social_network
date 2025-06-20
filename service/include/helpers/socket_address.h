#pragma once

#include <string>
#include <array>
#include <vector>
#include <optional>
#include <memory>
#include <arpa/inet.h>
#include <sys/un.h>

namespace SocialNetwork {

namespace NetHelpers {

class IpAddress;

class SocketAddress
{
public:
    typedef enum class family_e : int {
        UNIX_LOCAL  = AF_UNIX,
        IPV4        = AF_INET,
        IPV6        = AF_INET6
    } family_t;

public:
    ~SocketAddress() = default;
    SocketAddress();
    SocketAddress(const SocketAddress&) = default;
    SocketAddress(SocketAddress&&) noexcept = default;
    SocketAddress& operator = (const SocketAddress&) = default;
    SocketAddress& operator = (SocketAddress&&) noexcept = default;

    explicit SocketAddress(const std::string& addr);    // примеры addr:
                                                        //      192.168.1.10:80
                                                        //      [::ffff:192.168.1.120]:2040
                                                        //      www.example.com:8080
                                                        //      /tmp/local.sock
    explicit SocketAddress(family_t family); // creates a wildcard (zero) SocketAddress with of the given family
    explicit SocketAddress(uint16_t port);   // creates a SocketAddress with wildcard (zero) IPv4 address and given port number
    SocketAddress(family_t family, uint16_t port); // with wildcard (zero) IpAddress and given port number and the given family
    SocketAddress(family_t family, const std::string& addr);
    SocketAddress(family_t family, const std::string& addr, uint16_t port);
    SocketAddress(family_t family, const std::string& addr, const std::string& port);
    SocketAddress(const std::string& addr, uint16_t port);           // addr must be in dotted decimal (IPv4) or hex string (IPv6) format, or a domain name
    SocketAddress(const std::string& addr, const std::string& port); // port must either be a decimal port number, or a service name

    SocketAddress(const IpAddress& host_ip, uint16_t port);
    SocketAddress(const struct sockaddr* sockaddr, size_t len);

    void swap(SocketAddress& rsh) noexcept;

    void clear();

    bool operator == (const SocketAddress& rsh) const;
    bool operator != (const SocketAddress& rsh) const;
    bool operator >  (const SocketAddress& rsh) const;
    bool operator >= (const SocketAddress& rsh) const;
    bool operator < (const SocketAddress& rsh) const;
    bool operator <= (const SocketAddress& rsh) const;

    bool is_ipv4() const;
    bool is_ipv6() const;
    bool is_unix_local() const;
    family_t family() const;
    int af() const;
    IpAddress host() const;
    uint16_t port() const;
    socklen_t length() const;
    const struct sockaddr* sockaddr() const;

    std::string to_string() const;

private:
    family_t                             family_{family_t::IPV4};
    std::unique_ptr<struct sockaddr_in>  ipv4_{nullptr};
    std::unique_ptr<struct sockaddr_in6> ipv6_{nullptr};
    std::unique_ptr<struct sockaddr_un>  unix_{nullptr};

    void init(const IpAddress& host_ip, uint16_t port);
    void init(family_t family, const std::string& addr, uint16_t port);
    void init(const std::string& addr, uint16_t port);
    void init(const std::string& path);
    void split_to_host_and_port(const std::string& addr, std::string& host, std::string& port);
};

inline void swap(SocketAddress& sa1, SocketAddress& sa2) noexcept
{
    sa1.swap(sa2);
}

} // namespace NetHelpers

} // namespace SocialNetwork
