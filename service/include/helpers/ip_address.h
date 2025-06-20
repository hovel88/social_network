#pragma once

#include <string>
#include <array>
#include <vector>
#include <optional>
#include <arpa/inet.h>

namespace SocialNetwork {

namespace NetHelpers {

class IpAddress
{
public:
    static constexpr size_t IPV4_SIZE = sizeof(in_addr);
    static constexpr size_t IPV6_SIZE = sizeof(in6_addr);
    static constexpr size_t IP_MAX_SIZE = IPV6_SIZE;

    using binary_v4 = std::array<uint8_t, IPV4_SIZE>;
    using binary_v6 = std::array<uint8_t, IPV6_SIZE>;
    using binary    = std::vector<uint8_t>;

    typedef enum class family_e : int {
        V4   = AF_INET,
        V6   = AF_INET6
    } family_t;

public:
    ~IpAddress() = default;
    IpAddress(); // creates a wildcard (zero) IPv4 IPAddress
    IpAddress(const IpAddress&) = default;
    IpAddress(IpAddress&&) noexcept = default;
    IpAddress& operator = (const IpAddress&) = default;
    IpAddress& operator = (IpAddress&&) noexcept = default;

    explicit IpAddress(const std::string& addr); // creates IP address in presentation format (dotted decimal for IPv4, hex string for IPv6)
    explicit IpAddress(family_t family); // creates a wildcard (zero) IPAddress for the given address family

    IpAddress(const uint8_t* raw, size_t raw_sz, family_t family);
    IpAddress(const uint16_t* raw, size_t raw_sz, family_t family);
    IpAddress(const uint32_t* raw, size_t raw_sz, family_t family);

    IpAddress(const void* in_addr, socklen_t len); // creates an IPAddress from a in_addr or a in6_addr structure
    explicit IpAddress(const struct in_addr& in_addr);
    explicit IpAddress(const struct in6_addr& in_addr);

    explicit IpAddress(const struct sockaddr& sockaddr);
    explicit IpAddress(const struct sockaddr_in& sockaddr);
    explicit IpAddress(const struct sockaddr_in6& sockaddr);

    void swap(IpAddress& ip) noexcept;

    void clear();

    bool operator == (const IpAddress& rsh) const;
    bool operator != (const IpAddress& rsh) const;
    bool operator >  (const IpAddress& rsh) const;
    bool operator >= (const IpAddress& rsh) const;
    bool operator <  (const IpAddress& rsh) const;
    bool operator <= (const IpAddress& rsh) const;
    IpAddress operator & (const IpAddress& rsh) const;
    IpAddress operator | (const IpAddress& rsh) const;
    IpAddress operator ^ (const IpAddress& rsh) const;
    IpAddress operator ~ () const;

    bool is_v4() const;
    bool is_v6() const;
    family_t family() const; // the address family (IPv4 or IPv6)
    int af() const;          // the address family (AF_INET or AF_INET6)

    binary_v4 to_binary_v4() const;
    binary_v6 to_binary_v6() const;
    binary data() const;
    socklen_t length() const;
    const void* addr() const;

    uint32_t get_htonl_v4() const;
    IpAddress& set_ntohl_v4(uint32_t addr);

    bool try_parse(const std::string& addr);
    bool try_parse(const std::string& addr, family_t family);
    std::string to_string() const;

    bool is_wildcard() const;
    bool is_broadcast() const;
    bool is_multicast() const;
    bool is_loopback() const;
    bool is_unicast() const;
    bool is_linklocal() const;
    bool is_sitelocal() const;
    bool is_reserved() const;
    bool is_documentation() const;

    // bool is_well_known_multicast() const;
    // bool is_nodelocal_multicast() const;
    // bool is_linklocal_multicast() const;
    // bool is_sitelocal_multicast() const;
    // bool is_orglocal_multicast() const;
    // bool is_global_multicast() const;

    bool is_ipv4_compatible() const;
    bool is_ipv4_mapped() const;

    bool increment();
    void inverse();
    void mask(const IpAddress& mask_ip);                          // IPv4 only, new address is (address & mask)
    void mask(const IpAddress& mask_ip, const IpAddress& set_ip); // IPv4 only, new address is (address & mask) | (set & ~mask)

    std::optional<IpAddress> get_v4_from_ipv4_mapped() const;
    std::optional<IpAddress> get_ipv4_mapped_from_ipv4() const;

private:
    family_t family_{family_t::V4};
    uint8_t  data_[IP_MAX_SIZE]{0};

    static std::optional<family_t> try_family(const std::string& addr);
    static bool is_v4_format(const std::string& addr);
    static bool is_v6_format(const std::string& addr);
    static std::string trim_v6_string(const std::string& addr);
};

inline void swap(IpAddress& ip1, IpAddress& ip2) noexcept
{
    ip1.swap(ip2);
}

} // namespace NetHelpers

} // namespace SocialNetwork
