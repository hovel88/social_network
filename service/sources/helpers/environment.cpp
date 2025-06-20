#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include "helpers/environment.h"

namespace SocialNetwork {

namespace EnvironmentHelpers {

bool has(const std::string& name)
{
    return std::getenv(name.c_str()) != nullptr;
}

std::optional<std::string> get(const std::string& name)
{
    const char* val = std::getenv(name.c_str());
    if (val) return std::string(val);
    return std::nullopt;
}

std::string get(const std::string& name, const std::string& def)
{
    return get(name).value_or(def);
}

std::string os_name()
{
    struct utsname uts{};
    uname(&uts);
    return uts.sysname;
}

std::string os_version()
{
    struct utsname uts{};
    uname(&uts);
    return uts.release;
}

std::string os_architecture()
{
    struct utsname uts{};
    uname(&uts);
    return uts.machine;
}

std::optional<std::string> node_id()
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        return std::nullopt;
    }

    // начинаем подбирать буфер подходящего размера,
    // чтобы вместить все данные о физических интерфейсах
    int   len = 100*sizeof(struct ifreq);
    char* buf = nullptr;
    struct ifconf ifc;
    int last_len = 0;
    for (;;) {
        buf = new char[len];
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
            if (errno != EINVAL || last_len != 0) {
                close(sock);
                delete [] buf;
                return std::nullopt;
            }
        } else {
            // подобрали буфер подходящего размера
            if (ifc.ifc_len == last_len) break;
            last_len = ifc.ifc_len;
        }
        // буфер недостаточного размера, добавим еще
        len += 10*sizeof(struct ifreq);
        delete [] buf;
    }

    char id[sizeof("00:00:00:00:00:00")] = {0};
    bool success = false;
    for (const char* ptr = buf; ptr < buf + ifc.ifc_len;) {
        const struct ifreq* ifr = reinterpret_cast<const struct ifreq*>(ptr);
        if (ioctl(sock, SIOCGIFFLAGS, ifr) < 0) {
            // XXX: надо ли прерывать или попытаемся пройти по всем?
        } else {
            // loopback не нужен
            if (! (ifr->ifr_flags & IFF_LOOPBACK)) {
                if (ioctl(sock, SIOCGIFHWADDR, ifr) < 0) {
                    // XXX: надо ли прерывать или попытаемся пройти по всем?
                } else {
                    const struct sockaddr* sa = reinterpret_cast<const struct sockaddr*>(&ifr->ifr_hwaddr);
                    if (sa->sa_family == ARPHRD_ETHER) {
                        std::snprintf(id, sizeof(id), "%02x:%02x:%02x:%02x:%02x:%02x",
                            sa->sa_data[0], sa->sa_data[1], sa->sa_data[2],
                            sa->sa_data[3], sa->sa_data[4], sa->sa_data[5]);
                        success = true;
                        break;
                    }
                }
            }
        }
        ptr += sizeof(struct ifreq);
    }
    close(sock);
    delete [] buf;

    if (success) return std::string(id);
    return std::nullopt;
}

std::string node_name()
{
    struct utsname uts{};
    uname(&uts);
    return uts.nodename;
}

int processor_count()
{
#if defined(_SC_NPROCESSORS_ONLN)
    auto count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0) count = 1;
    return static_cast<int>(count);
#else
    return 1;
#endif
}

} // namespace EnvironmentHelpers

} // namespace SocialNetwork
