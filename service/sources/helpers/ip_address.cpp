#include <format>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <arpa/inet.h>
#include <net/if.h>
#include "helpers/string.h"
#include "helpers/ip_address.h"

namespace SocialNetwork {

namespace NetHelpers {

IpAddress::IpAddress()
{
    clear();
}

IpAddress::IpAddress(const std::string& addr)
{
    if (!try_parse(addr)) {
        throw std::runtime_error("invalid address presentation format passed to IpAddress()");
    }
}

IpAddress::IpAddress(family_t family)
{
    switch (family) {
    case family_t::V4:
    case family_t::V6:
        family_ = family;
        std::memset(data_, 0, sizeof(data_));
        break;
    default:
        throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");
        break;
    }
}

IpAddress::IpAddress(const uint8_t* raw, size_t raw_sz, family_t family)
{
    const size_t v4_expected_sz = IPV4_SIZE;
    const size_t v6_expected_sz = IPV6_SIZE;
    const uint8_t* raw_data     = raw;
    const size_t   raw_data_sz  = raw_sz;

    switch (family) {
    case family_t::V4:
    case family_t::V6:
        if (family == family_t::V4 && raw_sz != v4_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv4 address passed to IpAddress()");
        if (family == family_t::V6 && raw_sz != v6_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv6 address passed to IpAddress()");

        family_ = family;
        std::memcpy(data_, raw_data, raw_data_sz);
        break;
    default:
        throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");
        break;
    }
}

IpAddress::IpAddress(const uint16_t* raw, size_t raw_sz, family_t family)
{
    const size_t v4_expected_sz = IPV4_SIZE / 2;
    const size_t v6_expected_sz = IPV6_SIZE / 2;
    const uint8_t* raw_data     = reinterpret_cast<const uint8_t*>(raw);
    const size_t   raw_data_sz  = raw_sz * 2;

    switch (family) {
    case family_t::V4:
    case family_t::V6:
        if (family == family_t::V4 && raw_sz != v4_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv4 address passed to IpAddress()");
        if (family == family_t::V6 && raw_sz != v6_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv6 address passed to IpAddress()");

        family_ = family;
        std::memcpy(data_, raw_data, raw_data_sz);
        break;
    default:
        throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");
        break;
    }
}
IpAddress::IpAddress(const uint32_t* raw, size_t raw_sz, family_t family)
{
    const size_t v4_expected_sz = IPV4_SIZE / 4;
    const size_t v6_expected_sz = IPV6_SIZE / 4;
    const uint8_t* raw_data     = reinterpret_cast<const uint8_t*>(raw);
    const size_t   raw_data_sz  = raw_sz * 4;

    switch (family) {
    case family_t::V4:
    case family_t::V6:
        if (family == family_t::V4 && raw_sz != v4_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv4 address passed to IpAddress()");
        if (family == family_t::V6 && raw_sz != v6_expected_sz)
            throw std::runtime_error("invalid raw data size for IPv6 address passed to IpAddress()");

        family_ = family;
        std::memcpy(data_, raw_data, raw_data_sz);
        break;
    default:
        throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");
        break;
    }
}

IpAddress::IpAddress(const void* in_addr, socklen_t len)
{
    if (in_addr == nullptr) throw std::runtime_error("null address pointer passed to IpAddress()");

    if (len == sizeof(struct in_addr)) {
        const struct in_addr* addr = reinterpret_cast<const struct in_addr*>(in_addr);
        family_ = family_t::V4;
        *(reinterpret_cast<uint32_t*>(data_)) = ntohl(addr->s_addr);
    } else
    if (len == sizeof(struct in6_addr)) {
        const struct in6_addr* addr = reinterpret_cast<const struct in6_addr*>(in_addr);
        family_ = family_t::V6;
        std::memcpy(data_, addr, IPV6_SIZE);
    } else {
        throw std::runtime_error("invalid address length passed to IpAddress()");
    }
}

IpAddress::IpAddress(const struct in_addr& in_addr)
{
    family_ = family_t::V4;
    *(reinterpret_cast<uint32_t*>(data_)) = ntohl(in_addr.s_addr);
}

IpAddress::IpAddress(const struct in6_addr& in_addr)
{
    family_ = family_t::V6;
    std::memcpy(data_, &in_addr, IPV6_SIZE);
}

IpAddress::IpAddress(const struct sockaddr& sockaddr)
{
    switch (sockaddr.sa_family) {
    case AF_INET:
        family_ = family_t::V4;
        *(reinterpret_cast<uint32_t*>(data_)) = ntohl(reinterpret_cast<const struct sockaddr_in*>(&sockaddr)->sin_addr.s_addr);
        break;
    case AF_INET6:
        family_ = family_t::V6;
        std::memcpy(data_, &reinterpret_cast<const struct sockaddr_in6*>(&sockaddr)->sin6_addr, IPV6_SIZE);
        break;
    default:
        throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");
        break;
    }
}

IpAddress::IpAddress(const struct sockaddr_in& sockaddr)
{
    if (sockaddr.sin_family != AF_INET) throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");

    family_ = family_t::V4;
    *(reinterpret_cast<uint32_t*>(data_)) = ntohl(sockaddr.sin_addr.s_addr);
}

IpAddress::IpAddress(const struct sockaddr_in6& sockaddr)
{
    if (sockaddr.sin6_family != AF_INET6) throw std::runtime_error("invalid or unsupported address family passed to IpAddress()");

    family_ = family_t::V6;
    std::memcpy(data_, &sockaddr.sin6_addr, IPV6_SIZE);
}

void IpAddress::swap(IpAddress& ip) noexcept
{
    std::swap(family_, ip.family_);
    std::swap(data_, ip.data_);
}

void IpAddress::clear()
{
    family_ = family_t::V4;
    std::memset(data_, 0, sizeof(data_));
}

bool IpAddress::operator == (const IpAddress& rsh) const
{
    if (family() != rsh.family()) return false;

    size_t data_sz = length();
    if (std::memcmp(data_, rsh.data_, data_sz) == 0) {
        return true;
    }
    return false;
}

bool IpAddress::operator != (const IpAddress& rsh) const
{
    return !(*this == rsh);
}

bool IpAddress::operator > (const IpAddress& rsh) const
{
    if (family() > rsh.family()) return true;
    if (family() < rsh.family()) return false;

    size_t data_sz = length();
    if (std::memcmp(data_, rsh.data_, data_sz) > 0) {
        return true;
    }
    return false;
}

bool IpAddress::operator >= (const IpAddress& rsh) const
{
    if (*this == rsh) return true;
    return (*this > rsh);
}

bool IpAddress::operator < (const IpAddress &rsh) const
{
    if (*this == rsh) return false;
    return !(*this > rsh);
}

bool IpAddress::operator <= (const IpAddress& rsh) const
{
    if (*this == rsh) return true;
    return (*this < rsh);
}

IpAddress IpAddress::operator & (const IpAddress& rsh) const
{
    if (family() != rsh.family()) throw std::runtime_error("different address families");

    uint8_t ret_data[IP_MAX_SIZE] = {0};
    size_t data_sz = length();
    for (size_t i = 0; i < data_sz; ++i) {
        ret_data[i] = data_[i] & rsh.data_[i];
    }
    return IpAddress(ret_data, data_sz, family_);
}

IpAddress IpAddress::operator | (const IpAddress& rsh) const
{
    if (family() != rsh.family()) throw std::runtime_error("different address families");

    uint8_t ret_data[IP_MAX_SIZE] = {0};
    size_t data_sz = length();
    for (size_t i = 0; i < data_sz; ++i) {
        ret_data[i] = data_[i] | rsh.data_[i];
    }
    return IpAddress(ret_data, data_sz, family_);
}

IpAddress IpAddress::operator ^ (const IpAddress& rsh) const
{
    if (family() != rsh.family()) throw std::runtime_error("different address families");

    uint8_t ret_data[IP_MAX_SIZE] = {0};
    size_t data_sz = length();
    for (size_t i = 0; i < data_sz; ++i) {
        ret_data[i] = data_[i] ^ rsh.data_[i];
    }
    return IpAddress(ret_data, data_sz, family_);
}

IpAddress IpAddress::operator ~ () const
{
    uint8_t ret_data[IP_MAX_SIZE] = {0};
    size_t data_sz = length();
    for (size_t i = 0; i < data_sz; ++i) {
        ret_data[i] = ~data_[i];
    }
    return IpAddress(ret_data, data_sz, family_);
}

bool IpAddress::is_v4() const
{
    return family_ == family_t::V4;
}

bool IpAddress::is_v6() const
{
    return family_ == family_t::V6;
}

IpAddress::family_t IpAddress::family() const
{
    return family_;
}

int IpAddress::af() const
{
    return static_cast<int>(family_);
}

IpAddress::binary_v4 IpAddress::to_binary_v4() const
{
    if (!is_v4()) throw std::runtime_error("IpAddress as not an IPv4 address");
    IpAddress::binary_v4 bytes;
    std::memcpy(&bytes[0], data_, IPV4_SIZE);
    return bytes;
}

IpAddress::binary_v6 IpAddress::to_binary_v6() const
{
    if (!is_v6()) throw std::runtime_error("IpAddress as not an IPv6 address");
    IpAddress::binary_v6 bytes;
    std::memcpy(&bytes[0], data_, IPV6_SIZE);
    return bytes;
}

IpAddress::binary IpAddress::data() const
{
    if (is_v4()) {
        return IpAddress::binary(data_, data_ + IPV4_SIZE);
    }
    if (is_v6()) {
        return IpAddress::binary(data_, data_ + IPV6_SIZE);
    }
    return {};
}

socklen_t IpAddress::length() const
{
    if (is_v6()) return IPV6_SIZE;
    return IPV4_SIZE;
}

const void* IpAddress::addr() const
{
    return data_;
}

uint32_t IpAddress::get_htonl_v4() const
{
    if (is_v4()) {
        return htonl(*(reinterpret_cast<const uint32_t*>(data_)));
    }
    return 0;
}

IpAddress& IpAddress::set_ntohl_v4(uint32_t addr)
{
    family_ = family_t::V4;
    *(reinterpret_cast<uint32_t*>(data_)) = ntohl(addr);
    return *this;
}

bool IpAddress::try_parse(const std::string& addr)
{
    auto family = try_family(addr);
    if (!family.has_value()) {
        return false;
    }
    return try_parse(addr, family.value());
}

bool IpAddress::try_parse(const std::string& addr, family_t family)
{
    family_ = family;
    memset(data_, 0, sizeof(data_));

    std::string addr_str = StringHelpers::trim(addr);
    if (addr_str.empty()) {
        return true;
    }

    // man inet_pton
    //      1  on success (network address was successfully converted for the specified address family)
    //      -1 if some other error occurred
    //      0  if the address wasn't valid ('src' does not contain a character string representing
    //         a valid network address in the specified address family)
    if (family == family_t::V4) {
        if (inet_pton(static_cast<int>(family), addr_str.data(), data_) <= 0) {
            return false;
        }
        return true;
    }
    if (family == family_t::V6) {
        std::string addr_v6_str = trim_v6_string(addr_str);
        // запись IPv6 адреса может быть в формате со scope после символа '%',
        // например: 'FF02::1:FF00:0%1'
        // также, часто формат записи адреса в виде строки в квадратных скобках,
        // например: '[FE80::1%1]'
        auto pos = addr_v6_str.find('%');
        if (pos != std::string::npos) {
            size_t start = (addr_v6_str[0] == '[') ? 1 : 0;
            std::string unscoped_addr(addr_v6_str, start, pos - start);

            // TODO: scope - это идентификатор сетевого интерфейса. сейчас я пока не знаю
            // зачем он может пригодиться нам, но если будет надо, то добавить обработку
            // std::string scope_str(addr_v6_str, pos + 1, addr_v6_str.size() - (2*start) - pos);
            // uint32_t scope_id = if_nametoindex(scope_str.c_str());

            if (inet_pton(static_cast<int>(family), unscoped_addr.c_str(), data_) <= 0) {
                return false;
            }
        } else {
            if (inet_pton(static_cast<int>(family), addr_v6_str.c_str(), data_) <= 0) {
                return false;
            }
        }
        return true;
    }
    return false;
}

std::string IpAddress::to_string() const
{
    if (is_v4()) {
        // например '255.255.255.255'
        return std::format("{}.{}.{}.{}", data_[0], data_[1], data_[2], data_[3]);
    }
    if (is_v6()) {
        const uint16_t* words = reinterpret_cast<const uint16_t*>(data_);
        std::string result;

        if ((is_ipv4_compatible() && !is_loopback())
        ||  (is_ipv4_mapped())) {
            result.reserve(24); // например: 'ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255'
            if (words[5] == 0) result.append("::");
            else               result.append("::ffff:");
            if (data_[12] == 0) {
                result.append(std::format("{}.{}.{}.{}", data_[12], data_[13], data_[14], data_[15]));
            } else {
                // only 0.0.0.0 can start with zero
            }
        } else {
            result.reserve(40); // 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'
            bool zero_sequence = false;
            int i = 0;
            while (i < 8) {
                if (!zero_sequence && words[i] == 0) {
                    int zi = i;
                    while (zi < 8 && words[zi] == 0) ++zi;
                    if (zi > i + 1) {
                        i = zi;
                        result.append(":");
                        zero_sequence = true;
                    }
                }
                if (i > 0) result.append(":");
                if (i < 8) result.append(std::format("{:4x}", htons(words[i++])));
            }
        }

        return result;
    }
    return {};
}

bool IpAddress::is_wildcard() const
{
    static const uint32_t v4_null_address   = 0x00000000; //0.0.0.0
    static const uint32_t v6_null_address[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000}; // ::
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( *value == v4_null_address );
        break;
    case family_t::V6:
        rv = ( (value[0] == v6_null_address[0]) &&
               (value[1] == v6_null_address[1]) &&
               (value[2] == v6_null_address[2]) &&
               (value[3] == v6_null_address[3]) );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_broadcast() const
{
    static const uint32_t v4_broadcast_address = 0xFFFFFFFF; //255.255.255.255
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( *value == v4_broadcast_address );
        break;
    case family_t::V6:
        rv = false;
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_multicast() const
{
    static const uint32_t v4_multicast_address   = 0xE0000000; // 224.0.0.0
    static const uint32_t v4_multicast_mask      = 0xF0000000; // 240.0.0.0 (only if leading address bits of 1110, /4 mask)
    static const uint32_t v6_multicast_address[] = {0xFF000000, 0x00000000, 0x00000000, 0x00000000}; // ff00::
    static const uint32_t v6_multicast_mask[]    = {0xFF000000, 0x00000000, 0x00000000, 0x00000000}; // /8
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( (*value & v4_multicast_mask) == v4_multicast_address );
        break;
    case family_t::V6:
        rv = ( (value[0] & v6_multicast_mask[0]) == v6_multicast_address[0] );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_loopback() const
{
    static const uint32_t v4_loopback_address   = 0x7F000000; //127.0.0.0
    static const uint32_t v4_loopback_mask      = 0xFF000000; //255.0.0.0
    static const uint32_t v6_loopback_address[] = {0x00000000, 0x00000000, 0x00000000, 0x00000001}; // ::1
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( (*value & v4_loopback_mask) == v4_loopback_address );
        break;
    case family_t::V6:
        if (is_ipv4_mapped()) {
            rv = ( (value[4] & v4_loopback_mask) == v4_loopback_address );
        } else {
            rv = ( (value[0] == v6_loopback_address[0]) &&
                   (value[1] == v6_loopback_address[1]) &&
                   (value[2] == v6_loopback_address[2]) &&
                   (value[3] == v6_loopback_address[3]) );
        }
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_unicast() const
{
    return !is_wildcard() && !is_broadcast() && !is_multicast();
}

bool IpAddress::is_linklocal() const
{
    static const uint32_t v4_linklocal_address   = 0xA9FE0000; //169.254.0.0
    static const uint32_t v4_linklocal_mask      = 0xFFFF0000; //255.255.0.0
    static const uint32_t v6_linklocal_address[] = {0xFE800000, 0x00000000, 0x00000000, 0x00000000}; // fe80::
    static const uint32_t v6_linklocal_mask[]    = {0xFFC00000, 0x00000000, 0x00000000, 0x00000000}; // /10
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( (*value & v4_linklocal_mask) == v4_linklocal_address );
        break;
    case family_t::V6:
        rv = ( (value[0] & v6_linklocal_mask[0]) == v6_linklocal_address[0] );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_sitelocal() const
{
    static const uint32_t v4_sitelocal_address0   = 0x0A000000; // 10.0.0.0
    static const uint32_t v4_sitelocal_mask0      = 0xFF000000; // 255.0.0.0
    static const uint32_t v4_sitelocal_address1   = 0xC0A80000; // 192.168.0.0
    static const uint32_t v4_sitelocal_mask1      = 0xFFFF0000; // 255.255.0.0
    static const uint32_t v4_sitelocal_from       = 0xAC100000; // 172.16.0.0
    static const uint32_t v4_sitelocal_until      = 0xAC1FFFFF; // 172.31.255.255
    static const uint32_t v6_sitelocal_address0[] = {0xFEC00000, 0x00000000, 0x00000000, 0x00000000}; // fec0::
    static const uint32_t v6_sitelocal_mask0[]    = {0xFFC00000, 0x00000000, 0x00000000, 0x00000000}; // /10
    static const uint32_t v6_sitelocal_address1[] = {0xFC000000, 0x00000000, 0x00000000, 0x00000000}; // fc00::
    static const uint32_t v6_sitelocal_mask1[]    = {0xFE000000, 0x00000000, 0x00000000, 0x00000000}; // /7
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        if (!rv) {
            rv = ( (*value & v4_sitelocal_mask0) == v4_sitelocal_address0 );
        }
        if (!rv) {
            rv = ( (*value & v4_sitelocal_mask1) == v4_sitelocal_address1 );
        }
        if (!rv) {
            rv = ( (*value >= v4_sitelocal_from) && (*value <= v4_sitelocal_until) );
        }
        break;
    case family_t::V6:
        if (!rv) {
            rv = ( (value[0] & v6_sitelocal_mask0[0]) == v6_sitelocal_address0[0] );
        }
        if (!rv) {
            rv = ( (value[0] & v6_sitelocal_mask1[0]) == v6_sitelocal_address1[0] );
        }
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_reserved() const
{
    static const uint32_t v4_reserved_address = 0xF0000000; //240.0.0.0
    static const uint32_t v4_reserved_mask    = 0xF0000000; //240.0.0.0
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = ( (*value & v4_reserved_mask) == v4_reserved_address );
        break;
    case family_t::V6:
        rv = false;
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_documentation() const
{
    static const uint32_t v4_documentation_address0  = 0xC6336400; //198.51.100.0
    static const uint32_t v4_documentation_address1  = 0xCB007100; //203.0.113.0
    static const uint32_t v4_documentation_address2  = 0xC0000200; //192.0.2.0
    static const uint32_t v4_documentation_mask      = 0xFFFFFF00; //255.255.255.0
    static const uint32_t v6_documentation_address[] = {0x20010DB8, 0x00000000, 0x00000000, 0x00000000}; // 2001:db8::
    static const uint32_t v6_documentation_mask[]    = {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000}; // /32
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        if (!rv) {
            rv = ( (*value & v4_documentation_mask) == v4_documentation_address0 );
        }
        if (!rv) {
            rv = ( (*value & v4_documentation_mask) == v4_documentation_address1 );
        }
        if (!rv) {
            rv = ( (*value & v4_documentation_mask) == v4_documentation_address2 );
        }
        break;
    case family_t::V6:
        rv = ( (value[0] & v6_documentation_mask[0]) == v6_documentation_address[0] );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_ipv4_compatible() const
{
    static const uint32_t ipv4_compatible_ipv6_address[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000}; // ::x:x
    static const uint32_t ipv4_compatible_ipv6_mask[]    = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000}; // /96
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = true;
        break;
    case family_t::V6:
        rv = ( ((value[0] & ipv4_compatible_ipv6_mask[0]) == ipv4_compatible_ipv6_address[0]) &&
               ((value[1] & ipv4_compatible_ipv6_mask[1]) == ipv4_compatible_ipv6_address[1]) &&
               ((value[2] & ipv4_compatible_ipv6_mask[2]) == ipv4_compatible_ipv6_address[2]) );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::is_ipv4_mapped() const
{
    static const uint32_t ipv4_mapped_ipv6_address[] = {0x00000000, 0x00000000, 0x0000FFFF, 0x00000000}; // ::ffff:x.x.x.x
    static const uint32_t ipv4_mapped_ipv6_mask[]    = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000}; // /96
    const uint32_t* value = reinterpret_cast<const uint32_t*>(data_);
    bool rv = false;

    switch (family_) {
    case family_t::V4:
        rv = false;
        break;
    case family_t::V6:
        rv = ( ((value[0] & ipv4_mapped_ipv6_mask[0]) == ipv4_mapped_ipv6_address[0]) &&
               ((value[1] & ipv4_mapped_ipv6_mask[1]) == ipv4_mapped_ipv6_address[1]) &&
               ((value[2] & ipv4_mapped_ipv6_mask[2]) == ipv4_mapped_ipv6_address[2]) );
        break;
    default:
        break;
    };

    return rv;
}

bool IpAddress::increment()
{
    size_t data_sz = length();
    bool overflow  = false;
    uint32_t sum   = 0;
    for (int i = static_cast<int>(data_sz - 1); i >= 0; --i) {
        if (i == static_cast<int>(data_sz - 1)) {
            sum = data_[i] + 1;
        } else {
            if (overflow) sum = data_[i] + 1;
            else break;
        }

        if (sum & 0x0100) {
            overflow = true;
            data_[i] = 0;
        } else {
            overflow = false;
            data_[i] = static_cast<uint8_t>(0xFF & sum);
        }
    }
    return true;
}

void IpAddress::inverse()
{
    size_t data_sz = length();
    for (size_t i = 0; i < data_sz; ++i) {
        data_[i] = ~data_[i];
    }
}

void IpAddress::mask(const IpAddress& mask_ip)
{
    IpAddress null_ip;
    mask(mask_ip, null_ip);
}

void IpAddress::mask(const IpAddress& mask_ip, const IpAddress& set_ip)
{
    if (family_ != family_t::V4) return;
    if (family_ != mask_ip.family_
    ||  family_ != set_ip.family_) return;

    uint32_t* self_value = reinterpret_cast<uint32_t*>(data_);
    const uint32_t* mask_value = reinterpret_cast<const uint32_t*>(mask_ip.data_);
    const uint32_t* set_value  = reinterpret_cast<const uint32_t*>(set_ip.data_);

    *self_value &= *mask_value;
    *self_value |= (*set_value & ~*mask_value);
}

std::optional<IpAddress> IpAddress::get_v4_from_ipv4_mapped() const
{
    if (is_ipv4_mapped()) {
        const uint32_t* ipv6_data = reinterpret_cast<const uint32_t*>(data_);

        const size_t raw_sz = 1;
        const uint32_t* raw = &(ipv6_data[3]);

        IpAddress ipv4(raw, raw_sz, family_t::V4);
        return ipv4;
    }
    return std::nullopt;
}

std::optional<IpAddress> IpAddress::get_ipv4_mapped_from_ipv4() const
{
    if (is_v4()) {
        const uint32_t* ipv4_data = reinterpret_cast<const uint32_t*>(data_);

        const size_t raw_sz = 4;
        uint32_t raw[] = {0x00000000, 0x00000000, 0x0000FFFF, 0x00000000};
        raw[3] = ipv4_data[0];

        IpAddress ipv6(raw, raw_sz, family_t::V6);
        return ipv6;
    }
    return std::nullopt;
}

std::optional<IpAddress::family_t> IpAddress::try_family(const std::string& addr)
{
    bool v4 = is_v4_format(addr);
    bool v6 = is_v6_format(addr);
    if (v4 && !v6) return family_t::V4;
    if (!v4 && v6) return family_t::V6;
    return std::nullopt;
}

bool IpAddress::is_v4_format(const std::string& addr)
{
    // в корректном IPv4 адресе должно быть ТРИ разделителя в виде '.',
    // например: '192.168.136.112'
    auto separators = std::count(addr.begin(), addr.end(), '.');
    return (separators == 3);
}

bool IpAddress::is_v6_format(const std::string& addr)
{
    // в корректном IPv6 адресе должно быть СЕМЬ разделителей в виде ':',
    // например: '2001:0db8:85a3:0000:0000:8a2e:0370:7334'
    // но может быть и меньше, если задана сокращенная форма записи,
    // например: 'FF02::1:FF00:0'
    // в особом случае корректная запись адреса может быть вида: '::'
    auto separators = std::count(addr.begin(), addr.end(), ':');
    if (separators == 7) {
        return true; // полная запись
    }
    if (separators < 7
    &&  addr.find("::") != std::string::npos) {
        return true; // короткая запись
    }
    return false;
}

std::string IpAddress::trim_v6_string(const std::string& addr)
{
    std::string v6_addr(addr);
    std::string::size_type len = v6_addr.length();

    int double_separators = 0;
    auto pos = v6_addr.find("::");
    while ((pos <= len-2) && (pos != std::string::npos)) {
        ++double_separators;
        pos = v6_addr.find("::", pos + 2);
    }

    if ((double_separators > 1)
    ||  ((len >= 2) && ((v6_addr[len-1] == ':') && v6_addr[len-2] != ':'))) {
        return v6_addr;
    }

    while (v6_addr.size() && v6_addr[0] == '0') {
        // убираем лидирующие нули цифр в первом блоке
        v6_addr.erase(v6_addr.begin());
    }

    while (v6_addr.find(":0") != std::string::npos) {
        // убираем лидирующие нули цифр в последующих блоках
        StringHelpers::replace_in_place(v6_addr, ":0", ":");
    }

    while (v6_addr.find(":::") != std::string::npos) {
        // по итогу может получиться лишнее количество разделителей
        StringHelpers::replace_in_place(v6_addr, ":::", "::");
    }

    return v6_addr;
}

} // namespace NetHelpers

} // namespace SocialNetwork
