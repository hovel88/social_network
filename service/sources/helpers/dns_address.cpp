#include <format>
#include <cstring>
#include <algorithm>
#include <set>
#include <unistd.h>
#include <resolv.h>
#include "helpers/number_parser.h"
#include "helpers/ip_address.h"
#include "helpers/socket_address.h"
#include "helpers/dns_address.h"

namespace SocialNetwork {

namespace NetHelpers {

template <typename T>
void remove_duplicates(std::vector<T>& list)
{
    std::set<T> unique_values;
    // remove duplicates and preserve order
    list.erase(
        std::remove_if(list.begin(), list.end(), [&unique_values](const T& v) { return !unique_values.insert(v).second; }),
        list.end()
    );
}

HostEntry::HostEntry(struct hostent* entry)
{
    name_ = entry->h_name;

    char** alias = entry->h_aliases;
    if (alias) {
        while (*alias) {
            aliases_.push_back(std::string(*alias));
            ++alias;
        }
    }
    remove_duplicates(aliases_);

    char** address = entry->h_addr_list;
    if (address) {
        while (*address) {
            addresses_.push_back(IpAddress(*address, entry->h_length));
            ++address;
        }
    }
    remove_duplicates(addresses_);
}

HostEntry::HostEntry(struct addrinfo* info)
{
    for (struct addrinfo* ai = info; ai; ai = ai->ai_next) {
        if (ai->ai_canonname) {
            name_.assign(ai->ai_canonname);
        }
        if (ai->ai_addrlen && ai->ai_addr) {
            if (ai->ai_addr->sa_family == AF_INET
            ||  ai->ai_addr->sa_family == AF_INET6) {
                addresses_.push_back(IpAddress(*(ai->ai_addr)));
            }
        }
    }
    remove_duplicates(addresses_);
}

void HostEntry::swap(HostEntry& he) noexcept
{
    std::swap(name_, he.name_);
    std::swap(aliases_, he.aliases_);
    std::swap(addresses_, he.addresses_);
}

const std::string& HostEntry::name() const
{
    return name_;
}

const HostEntry::alias_list& HostEntry::aliases() const
{
    return aliases_;
}

const HostEntry::address_list& HostEntry::addresses() const
{
    return addresses_;
}



HostEntry Dns::host_by_name(const std::string& hostname, uint16_t flags)
{
    struct addrinfo* ai;
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_flags = flags;
    int rc = getaddrinfo(hostname.c_str(), NULL, &hints, &ai);
    if (rc == 0) {
        HostEntry result(ai);
        freeaddrinfo(ai);
        return result;
    }

    switch (rc) {
    case EAI_AGAIN:  throw DnsException(std::string("temporary DNS error while resolving '") + hostname + std::string("'"));
    case EAI_FAIL:   throw DnsException(std::string("non recoverable DNS error while resolving '") + hostname + std::string("'"));
    case EAI_NONAME: throw DnsException(std::string("host not found for '") + hostname + std::string("'"));
    case EAI_NODATA: throw DnsException(std::string("no address found for '") + hostname + std::string("'"));
    case EAI_SYSTEM:
        switch (h_errno) {
        case -4: throw DnsException(std::string("net subsystem not ready"));
        case -5: throw DnsException(std::string("net subsystem not initialized"));
        case TRY_AGAIN:      throw DnsException(std::string("temporary DNS error while resolving '") + hostname + std::string("'"));
        case NO_RECOVERY:    throw DnsException(std::string("non recoverable DNS error while resolving '") + hostname + std::string("'"));
        case HOST_NOT_FOUND: throw DnsException(std::string("host not found for '") + hostname + std::string("'"));
        case NO_DATA:        throw DnsException(std::string("no address found for '") + hostname + std::string("'"));
        default: break;
        }
    default: break;
    }

    struct hostent* he = gethostbyname(hostname.c_str());
    if (he) {
        return HostEntry(he);
    }

    switch (h_errno) {
    case -4: throw DnsException(std::string("net subsystem not ready"));
    case -5: throw DnsException(std::string("net subsystem not initialized"));
    case TRY_AGAIN:      throw DnsException(std::string("temporary DNS error while resolving '") + hostname + std::string("'"));
    case NO_RECOVERY:    throw DnsException(std::string("non recoverable DNS error while resolving '") + hostname + std::string("'"));
    case HOST_NOT_FOUND: throw DnsException(std::string("host not found for '") + hostname + std::string("'"));
    case NO_DATA:        throw DnsException(std::string("no address found for '") + hostname + std::string("'"));
    default: break;
    }

    // до сюда дойти не должно
    throw DnsException(std::string("net exception"));
}

HostEntry Dns::host_by_address(const IpAddress& address, uint16_t flags)
{
    SocketAddress sa(address, 0);
    char fqname[1024];
    int rc = getnameinfo(sa.sockaddr(), sa.length(), fqname, sizeof(fqname), NULL, 0, NI_NAMEREQD);
    if (rc == 0) {
        struct addrinfo* ai;
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_flags = flags;
        rc = getaddrinfo(fqname, NULL, &hints, &ai);
        if (rc == 0) {
            HostEntry result(ai);
            freeaddrinfo(ai);
            return result;
        }
    }

    switch (rc) {
    case EAI_AGAIN:  throw DnsException(std::string("temporary DNS error while resolving '") + address.to_string() + std::string("'"));
    case EAI_FAIL:   throw DnsException(std::string("non recoverable DNS error while resolving '") + address.to_string() + std::string("'"));
    case EAI_NONAME: throw DnsException(std::string("host not found for '") + address.to_string() + std::string("'"));
    case EAI_NODATA: throw DnsException(std::string("no address found for '") + address.to_string() + std::string("'"));
    case EAI_SYSTEM:
        switch (h_errno) {
        case -4: throw DnsException(std::string("net subsystem not ready"));
        case -5: throw DnsException(std::string("net subsystem not initialized"));
        case TRY_AGAIN:      throw DnsException(std::string("temporary DNS error while resolving '") + address.to_string() + std::string("'"));
        case NO_RECOVERY:    throw DnsException(std::string("non recoverable DNS error while resolving '") + address.to_string() + std::string("'"));
        case HOST_NOT_FOUND: throw DnsException(std::string("host not found for '") + address.to_string() + std::string("'"));
        case NO_DATA:        throw DnsException(std::string("no address found for '") + address.to_string() + std::string("'"));
        default: break;
        }
    default: break;
    }

    struct hostent* he = gethostbyaddr(reinterpret_cast<const char*>(address.addr()), address.length(), address.af());
    if (he) {
        return HostEntry(he);
    }

    switch (h_errno) {
    case -4: throw DnsException(std::string("net subsystem not ready"));
    case -5: throw DnsException(std::string("net subsystem not initialized"));
    case TRY_AGAIN:      throw DnsException(std::string("temporary DNS error while resolving '") + address.to_string() + std::string("'"));
    case NO_RECOVERY:    throw DnsException(std::string("non recoverable DNS error while resolving '") + address.to_string() + std::string("'"));
    case HOST_NOT_FOUND: throw DnsException(std::string("host not found for '") + address.to_string() + std::string("'"));
    case NO_DATA:        throw DnsException(std::string("no address found for '") + address.to_string() + std::string("'"));
    default: break;
    }

    // до сюда дойти не должно
    throw DnsException(std::string("net exception"));
}

HostEntry Dns::resolve(const std::string& address)
{
    IpAddress ip;
    if (ip.try_parse(address)) {
        return host_by_address(ip);
    } else {
        return host_by_name(address);
    }
}

IpAddress Dns::resolve_one(const std::string& address)
{
    const HostEntry& entry = resolve(address);
    if (!entry.addresses().empty()) {
        return entry.addresses()[0];
    } else {
        throw DnsException(std::string("no address found for '") + address + std::string("'"));
    }
}

uint16_t Dns::resolve_service_to_port(const std::string& service)
{
    uint32_t port = 0;
    if (NumberParserHelpers::try_parse_uint(service, port)
    &&  port <= 0xFFFF) {
        return static_cast<uint16_t>(port);
    }

    struct servent* se = getservbyname(service.c_str(), NULL);
    if (se) {
        return ntohs(static_cast<uint16_t>(se->s_port));
    } else {
        throw DnsException(std::string("service porn not found by service name = '") + service + std::string("'"));
    }
}

HostEntry Dns::this_host()
{
    return host_by_name(this_host_name());
}

std::string Dns::this_host_name()
{
    char buffer[256];
    int rc = gethostname(buffer, sizeof(buffer));
    if (rc == 0) {
        return std::string(buffer);
    } else {
        throw DnsException(std::string("cannot get host name"));
    }
}

void Dns::reload()
{
    res_init();
}

} // namespace NetHelpers

} // namespace SocialNetwork
