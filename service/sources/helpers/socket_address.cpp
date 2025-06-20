#include <format>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <net/if.h>
#include <netdb.h>
#include "helpers/ip_address.h"
#include "helpers/dns_address.h"
#include "helpers/socket_address.h"

namespace SocialNetwork {

namespace NetHelpers {

SocketAddress::SocketAddress(const std::string& addr)
{
    if (addr.size() && (addr[0] == '/')) {
        // UNIX local
        init(addr);
    } else {
        std::string host;
        std::string port;
        split_to_host_and_port(addr, host, port);
        init(host, Dns::resolve_service_to_port(port));
    }
}

SocketAddress::SocketAddress(family_t family)
:   SocketAddress(family, 0)
{
}

SocketAddress::SocketAddress(uint16_t port)
:   SocketAddress(family_t::IPV4, port)
{
}

SocketAddress::SocketAddress(family_t family, uint16_t port)
{
    if (family == family_t::IPV4) {
        init(IpAddress(IpAddress::family_t::V4), port);
    } else
    if (family == family_t::IPV6) {
        init(IpAddress(IpAddress::family_t::V6), port);
    } else {
        throw std::runtime_error("unsupported IP address family");
    }
}

SocketAddress::SocketAddress(family_t family, const std::string& addr)
{
    if (family == family_t::UNIX_LOCAL) {
        if (addr.size() && (addr[0] == '/')) {
            // UNIX local
            init(addr);
        } else {
            throw std::runtime_error("invalid UNIX local address");
        }
    } else {
        std::string host;
        std::string port;
        split_to_host_and_port(addr, host, port);
        init(family, host, Dns::resolve_service_to_port(port));
    }
}

SocketAddress::SocketAddress(family_t family, const std::string& addr, uint16_t port)
{
    init(family, addr, port);
}

SocketAddress::SocketAddress(family_t family, const std::string& addr, const std::string& port)
{
    init(family, addr, Dns::resolve_service_to_port(port));
}

SocketAddress::SocketAddress(const std::string& addr, uint16_t port)
{
    init(addr, port);
}

SocketAddress::SocketAddress(const std::string& addr, const std::string& port)
{
    init(addr, Dns::resolve_service_to_port(port));
}

SocketAddress::SocketAddress(const IpAddress& host_ip, uint16_t port)
{
    init(host_ip, port);
}

SocketAddress::SocketAddress(const struct sockaddr* sockaddr, size_t len)
{
    if (sockaddr
    &&  sockaddr->sa_family == AF_INET
    &&  len == sizeof(struct sockaddr_in)) {

        family_ = family_t::IPV4;
        ipv4_   = std::make_unique<struct sockaddr_in>();
        std::memcpy(ipv4_.get(), sockaddr, sizeof(struct sockaddr_in));

    } else
    if (sockaddr
    &&  sockaddr->sa_family == AF_INET6
    &&  len == sizeof(struct sockaddr_in6)) {

        family_ = family_t::IPV6;
        ipv6_   = std::make_unique<struct sockaddr_in6>();
        std::memcpy(ipv6_.get(), sockaddr, sizeof(struct sockaddr_in6));

    } else
    if (sockaddr
    &&  sockaddr->sa_family == AF_UNIX
    &&  (len > 0 && len <= sizeof(struct sockaddr_un))) {

        family_ = family_t::UNIX_LOCAL;
        unix_   = std::make_unique<struct sockaddr_un>();
        std::memcpy(unix_.get(), sockaddr, sizeof(struct sockaddr_un));

    } else {

        throw std::runtime_error("invalid address length or family passed to SocketAddress()");

    }
}

void SocketAddress::swap(SocketAddress& rsh) noexcept
{
    std::swap(family_, rsh.family_);
    std::swap(ipv4_, rsh.ipv4_);
    std::swap(ipv6_, rsh.ipv6_);
    std::swap(unix_, rsh.unix_);
}

bool SocketAddress::operator == (const SocketAddress& rsh) const
{
    if (family() == family_t::UNIX_LOCAL) {
        return to_string() == rsh.to_string();
    }
    return host() == rsh.host()
        && port() == rsh.port();
}

bool SocketAddress::operator != (const SocketAddress& rsh) const
{
    return !(*this == rsh);
}

bool SocketAddress::operator >  (const SocketAddress& rsh) const
{
    if (family() > rsh.family()) return true;
    if (family() < rsh.family()) return false;

    if (family() == family_t::UNIX_LOCAL) {
        return to_string() > rsh.to_string();
    }

    if (host() > rsh.host()) return true;
    if (host() < rsh.host()) return false;

    return (port() > rsh.port());
}

bool SocketAddress::operator >= (const SocketAddress& rsh) const
{
    if (*this == rsh) return true;
    return (*this > rsh);
}

bool SocketAddress::operator < (const SocketAddress& rsh) const
{
    if (*this == rsh) return false;
    return !(*this > rsh);
}

bool SocketAddress::operator <= (const SocketAddress& rsh) const
{
    if (*this == rsh) return true;
    return (*this < rsh);
}

bool SocketAddress::is_ipv4() const
{
    return family_ == family_t::IPV4
        && ipv4_   != nullptr;
}

bool SocketAddress::is_ipv6() const
{
    return family_ == family_t::IPV6
        && ipv6_   != nullptr;
}

bool SocketAddress::is_unix_local() const
{
    return family_ == family_t::UNIX_LOCAL
        && unix_   != nullptr;
}

SocketAddress::family_t SocketAddress::family() const
{
    return family_;
}

int SocketAddress::af() const
{
    if (is_ipv4()) {
        return ipv4_->sin_family;
    } else
    if (is_ipv6()) {
        return ipv6_->sin6_family;
    } else
    if (is_unix_local()) {
        return unix_->sun_family;
    }
    return 0;
}

IpAddress SocketAddress::host() const
{
    if (is_ipv4()) {
        return IpAddress(*ipv4_);
    } else
    if (is_ipv6()) {
        return IpAddress(*ipv6_);
    } else
    if (is_unix_local()) {
        throw std::runtime_error("local socket address does not have host IP address");
    }
    return IpAddress();
}

uint16_t SocketAddress::port() const
{
    if (is_ipv4()) {
        return ntohs(ipv4_->sin_port);
    } else
    if (is_ipv6()) {
        return ntohs(ipv6_->sin6_port);
    } else
    if (is_unix_local()) {
        throw std::runtime_error("local socket address does not have port number");
    }
    return 0;
}

socklen_t SocketAddress::length() const
{
    if (is_ipv4()) {
        return sizeof(struct sockaddr_in);
    } else
    if (is_ipv6()) {
        return sizeof(struct sockaddr_in6);
    } else
    if (is_unix_local()) {
        return sizeof(struct sockaddr_un);
    }
    return 0;
}

const struct sockaddr* SocketAddress::sockaddr() const
{
    if (is_ipv4()) {
        return reinterpret_cast<const struct sockaddr*>(ipv4_.get());
    } else
    if (is_ipv6()) {
        return reinterpret_cast<const struct sockaddr*>(ipv6_.get());
    } else
    if (is_unix_local()) {
        return reinterpret_cast<const struct sockaddr*>(unix_.get());
    }
    return nullptr;
}

std::string SocketAddress::to_string() const
{
    if (is_ipv4()) {
        return std::format("{}:{}", host().to_string(), port());
    }
    if (is_ipv6()) {
        return std::format("[{}]:{}", host().to_string(), port());
    }
    if (is_unix_local()) {
        return std::string(unix_->sun_path);
    }
    return {};
}

void SocketAddress::init(const IpAddress& host_ip, uint16_t port)
{
    if (host_ip.family() == IpAddress::family_t::V4) {

        family_ = family_t::IPV4;
        ipv4_   = std::make_unique<struct sockaddr_in>();

        std::memset(ipv4_.get(), 0, sizeof(struct sockaddr_in));
        ipv4_->sin_family = AF_INET;
        ipv4_->sin_port = htons(port);
        std::memcpy(&ipv4_->sin_addr, host_ip.addr(), sizeof(ipv4_->sin_addr));

    } else
    if (host_ip.family() == IpAddress::family_t::V6) {

        family_ = family_t::IPV6;
        ipv6_   = std::make_unique<struct sockaddr_in6>();

        std::memset(ipv6_.get(), 0, sizeof(struct sockaddr_in6));
        ipv6_->sin6_family = AF_INET6;
        ipv6_->sin6_port = htons(port);
        std::memcpy(&ipv6_->sin6_addr, host_ip.addr(), sizeof(ipv6_->sin6_addr));

    } else {

        throw std::runtime_error("unsupported IP address family");

    }
}

void SocketAddress::init(family_t family, const std::string& addr, uint16_t port)
{
    if (family == family_t::UNIX_LOCAL) {
        throw std::runtime_error(std::string("unexpected IP address family"));
    }

    IpAddress::family_t ip_family = ( (family == family_t::IPV4) ? IpAddress::family_t::V4 : IpAddress::family_t::V6 );
    IpAddress ip;
    if (ip.try_parse(addr)) {
        if (ip_family != ip.family()) {
            throw std::runtime_error(std::string("mismatch IP address family"));
        }
        init(ip, port);
    } else {
        HostEntry he = Dns::host_by_name(addr);
        HostEntry::address_list addresses = he.addresses();
        if (addresses.size() > 0) {
            for (const auto& addr: addresses) {
                if (addr.family() == ip_family) {
                    init(addr, port);
                    return;
                }
            }
            throw std::runtime_error(std::string("addresses found, but address family mismatch for host '") + addr + std::string("'"));
        } else {
            throw std::runtime_error(std::string("no address found for host '") + addr + std::string("'"));
        }
    }
}

void SocketAddress::init(const std::string& addr, uint16_t port)
{
    IpAddress ip;
    if (ip.try_parse(addr)) {
        init(ip, port);
    } else {
        HostEntry he = Dns::host_by_name(addr);
        HostEntry::address_list addresses = he.addresses();
        if (addresses.size() > 0) {
            std::stable_sort(addresses.begin(), addresses.end(), [](const IpAddress& ip1, const IpAddress& ip2) -> bool
                { return ip1.af() < ip2.af(); });

            init(addresses[0], port);
        } else {
            throw std::runtime_error(std::string("no address found for host '") + addr + std::string("'"));
        }
    }
}

void SocketAddress::init(const std::string& path)
{
    family_ = family_t::UNIX_LOCAL;
    unix_   = std::make_unique<struct sockaddr_un>();
    std::memset(unix_.get(), 0, sizeof(struct sockaddr_un));
    unix_->sun_family = AF_UNIX;
    std::memcpy(unix_->sun_path, path.c_str(), path.length());
}

void SocketAddress::split_to_host_and_port(const std::string& addr, std::string& host, std::string& port)
{
    std::string::const_iterator it  = addr.begin();
    std::string::const_iterator end = addr.end();
    if (*it == '[') {
        ++it;
        while (it != end && *it != ']') host += *it++;
        if (it == end) {
            throw std::runtime_error("malformed IPv6 address");
        }
        ++it;
    } else {
        while (it != end && *it != ':') host += *it++;
    }

    if (it != end && *it == ':') {
        ++it;
        while (it != end) port += *it++;
    }

    if (host.empty() || port.empty()) {
        throw std::runtime_error("missing host and/or port number");
    }
}

} // namespace NetHelpers

} // namespace SocialNetwork
