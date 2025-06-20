#pragma once

#include <string>
#include <array>
#include <vector>
#include <optional>
#include <stdexcept>
#include <netdb.h>

namespace SocialNetwork {

namespace NetHelpers {

class IpAddress;

class HostEntry
{
public:
    using alias_list   = std::vector<std::string>;
    using address_list = std::vector<IpAddress>;

public:
    ~HostEntry() = default;
    HostEntry() = default;
    HostEntry(const HostEntry&) = default;
    HostEntry(HostEntry&&) noexcept = default;
    HostEntry& operator = (const HostEntry&) = default;
    HostEntry& operator = (HostEntry&&) noexcept = default;

    HostEntry(struct hostent* entry);
    HostEntry(struct addrinfo* info);

    void swap(HostEntry& he) noexcept;

    const std::string& name() const;
    const alias_list& aliases() const;
    const address_list& addresses() const;

private:
    std::string  name_{};
    alias_list   aliases_{};
    address_list addresses_{};
};

inline void swap(HostEntry& h1, HostEntry& h2) noexcept
{
    h1.swap(h2);
}



class DnsException: public std::exception
{
public:
    virtual ~DnsException() noexcept {};
    DnsException() = default;
    DnsException(const DnsException&) = default;
    DnsException(DnsException&&) noexcept = default;
    DnsException& operator = (const DnsException&) = default;
    DnsException& operator = (DnsException&&) noexcept = default;

    explicit DnsException(const std::string& msg)
    :   msg_(msg) {}
    explicit DnsException(const char* msg)
    :   msg_(std::string(msg)) {}

    virtual const char* what() const noexcept { return msg_.c_str(); }

private:
    std::string msg_;
};

class Dns
{
public:
    static constexpr uint16_t DNS_HINT_NONE           = 0;
    static constexpr uint16_t DNS_HINT_AI_PASSIVE     = AI_PASSIVE;     // Socket address will be used in bind() call
    static constexpr uint16_t DNS_HINT_AI_CANONNAME   = AI_CANONNAME;   // Return canonical name in first ai_canonname
    static constexpr uint16_t DNS_HINT_AI_NUMERICHOST = AI_NUMERICHOST; // Nodename must be a numeric address string
    static constexpr uint16_t DNS_HINT_AI_NUMERICSERV = AI_NUMERICSERV; // Servicename must be a numeric port number
    static constexpr uint16_t DNS_HINT_AI_ALL         = AI_ALL;         // Query both IP6 and IP4 with AI_V4MAPPED
    static constexpr uint16_t DNS_HINT_AI_ADDRCONFIG  = AI_ADDRCONFIG;  // Resolution only if global address configured
    static constexpr uint16_t DNS_HINT_AI_V4MAPPED    = AI_V4MAPPED;    // On v6 failure, query v4 and convert to V4MAPPED format

public:
    static HostEntry host_by_name(const std::string& hostname, uint16_t flags = DNS_HINT_AI_CANONNAME | DNS_HINT_AI_ADDRCONFIG);
    static HostEntry host_by_address(const IpAddress& address, uint16_t flags = DNS_HINT_AI_CANONNAME | DNS_HINT_AI_ADDRCONFIG);

    static HostEntry resolve(const std::string& address);     // returns a HostEntry object containing the DNS information for the host with the given IpAddress or host name
    static IpAddress resolve_one(const std::string& address); // returns the first IpAddress from the resolve(address)
    static uint16_t resolve_service_to_port(const std::string& service);

    static HostEntry this_host();        // returns a HostEntry containing the DNS information for this host
    static std::string this_host_name(); // returns the host name of this host

    static void reload();
};

} // namespace NetHelpers

} // namespace SocialNetwork
