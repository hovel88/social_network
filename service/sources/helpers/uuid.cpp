#include <format>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <limits>
#include <string>
#include <string.h>
#include <stdexcept>
#include <stdint.h>
#include "helpers/uuid.h"

namespace UuidHelpers {

Uuid::Uuid()
{
    clear();
}

Uuid::Uuid(const Uuid& right)
{
    uuid_copy(raw_, right.raw_);
}

Uuid& Uuid::operator=(const Uuid& right)
{
    clear();
    uuid_copy(raw_, right.raw_);
    return *this;
}

Uuid::Uuid(const uuid_t& raw)
{
    uuid_copy(raw_, raw);
}

Uuid::Uuid(const char* uuid_str)
{
    if (!set(uuid_str)) {
        throw std::runtime_error(std::format("invalid UUID '{}'", (uuid_str != nullptr ? uuid_str : "NULL")));
    }
}

Uuid::Uuid(const std::string& uuid_str)
:   Uuid(uuid_str.c_str())
{
}

Uuid::Uuid(const binary& data)
{
    from_binary(data);
}

size_t Uuid::hash() const
{
    // Fowler–Noll–Vo FNV-1a hash function (https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function)
    static const uint64_t FNV_offset_basis = 14695981039346656037UL;
    static const uint64_t FNV_prime = 1099511628211;

    size_t result = FNV_offset_basis;
    for (size_t offset = 0; offset < sizeof(uuid_t); offset++) {
        result ^= raw_[offset];
        result *= FNV_prime;
    }
    return result;
}

void Uuid::swap(Uuid& uuid) noexcept
{
    std::swap(raw_, uuid.raw_);
}

void Uuid::clear()
{
    uuid_clear(raw_);
}

bool Uuid::is_null() const
{
    return uuid_is_null(raw_);
}

bool Uuid::try_get_time(struct timeval* tv) const
{
    struct timeval ret_tv;
    time_t ret = uuid_time(raw_, &ret_tv);
    if (tv) *tv = ret_tv;
    return ret > 0;
}

Uuid::variant_t Uuid::variant() const
{
    int v = uuid_variant(raw_);
    switch (v) {
    case 0: return Uuid::variant_t::variant_ncs;
    case 1: return Uuid::variant_t::variant_dce;
    case 2: return Uuid::variant_t::variant_microsoft;
    default:
        break;
    }
    return Uuid::variant_t::variant_other;
}

std::string Uuid::variant_str() const
{
    int v = uuid_variant(raw_);
    switch (v) {
    case 0: return std::string("NCS");
    case 1: return std::string("DCE");
    case 2: return std::string("Microsoft");
    default:
        break;
    }
    return std::string("Other");
}

Uuid::type_t Uuid::type() const
{
    int t = uuid_type(raw_);
    switch (t) {
    case 0: return Uuid::type_t::type_dce_nil;
    case 1: return Uuid::type_t::type_dce_time;
    case 2: return Uuid::type_t::type_dce_security;
    case 3: return Uuid::type_t::type_dce_md5;
    case 4: return Uuid::type_t::type_dce_random;
    case 5: return Uuid::type_t::type_dce_sha1;
    case 6: return Uuid::type_t::type_dce_time_v6;
    case 7: return Uuid::type_t::type_dce_time_v7;
    case 8: return Uuid::type_t::type_dce_vendor;
    default:
        break;
    }
    return Uuid::type_t::type_dce_other;
}

std::string Uuid::type_str() const
{
    int t = uuid_type(raw_);
    switch (t) {
    case 0: return std::string("(nil)");
    case 1: return std::string("(time based)");
    case 2: return std::string("(security DCE)");
    case 3: return std::string("(name-based MD5)");
    case 4: return std::string("(random)");
    case 5: return std::string("(name-based SHA1)");
    case 6: return std::string("(time based v6)");
    case 7: return std::string("(time based v7)");
    case 8: return std::string("(vendor)");
    default:
        break;
    }
    return std::string("Other");
}

bool Uuid::set(const char* uuid_str)
{
    return make_from_string(uuid_str, raw_);
}

bool Uuid::set(const std::string& uuid_str)
{
    return set(uuid_str.c_str());
}

Uuid::binary Uuid::to_binary() const
{
    Uuid::binary tmp;
    memcpy(tmp.data(), raw_, sizeof(uuid_t));
    return tmp;
}

void Uuid::from_binary(const binary& data)
{
    memcpy(raw_, data.data(), data.size());
}

std::string Uuid::to_string_lowercase() const
{
    char result[UUID_STR_LEN] = {0};
    uuid_unparse_lower(raw_, result);
    return std::string(result);
}

std::string Uuid::to_string_uppercase() const
{
    char result[UUID_STR_LEN] = {0};
    uuid_unparse_upper(raw_, result);
    return std::string(result);
}

bool Uuid::operator!=(const Uuid& right) const
{
    return uuid_compare(raw_, right.raw_) != 0;
}

bool Uuid::operator==(const Uuid& right) const
{
    return uuid_compare(raw_, right.raw_) == 0;
}

bool Uuid::operator<(const Uuid& right) const
{
    return uuid_compare(raw_, right.raw_) < 0;
}

bool Uuid::operator>(const Uuid& right) const
{
    return uuid_compare(raw_, right.raw_) > 0;
}

Uuid Uuid::null_based()
{
    Uuid tmp;
    uuid_clear(tmp.raw_);
    return tmp;
}

Uuid Uuid::time_based()
{
    Uuid tmp;
    uuid_generate_time(tmp.raw_);
    return tmp;
}

Uuid Uuid::random_based()
{
    Uuid tmp;
    uuid_generate_random(tmp.raw_);
    return tmp;
}

Uuid Uuid::dns_name_based(const std::string& name)
{
    Uuid tmp;
    make_from_namespace_md5(uuid_get_template("dns"), name, tmp.raw_);
    return tmp;
}

Uuid Uuid::oid_name_based(const std::string& name)
{
    Uuid tmp;
    make_from_namespace_md5(uuid_get_template("oid"), name, tmp.raw_);
    return tmp;
}
Uuid Uuid::uri_name_based(const std::string& name)
{
    Uuid tmp;
    make_from_namespace_md5(uuid_get_template("uri"), name, tmp.raw_);
    return tmp;
}

Uuid Uuid::x500_name_based(const std::string& name)
{
    Uuid tmp;
    make_from_namespace_md5(uuid_get_template("x500"), name, tmp.raw_);
    return tmp;
}

bool Uuid::is_valid(const char* uuid_str)
{
    uuid_t tmp;
    return make_from_string(uuid_str, tmp);
}

bool Uuid::is_valid(const std::string& uuid_str)
{
    return is_valid(uuid_str.c_str());
}

bool Uuid::make_from_string(const char* uuid_str, uuid_t& result)
{
    if (uuid_str != nullptr) {
        if (uuid_parse(uuid_str, result) == 0) {
            return true;
        }
    }
    uuid_clear(result);
    return false;
}

bool Uuid::make_from_namespace_md5(const uuid_t* uuid_ns, const std::string& name, uuid_t& result)
{
    if (uuid_ns) {
        auto ns = *uuid_ns;
        uuid_generate_md5(result, ns, name.c_str(), name.size());
        return true;
    }
    return false;
}

} // namespace UuidHelpers
