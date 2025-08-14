#pragma once

#include <string>
#include <array>
#include <uuid/uuid.h>

// MARK: - Typesafe enum bitfield support
//  - See https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html

namespace UuidHelpers {

class Uuid
{
public:
    typedef std::array<unsigned char, sizeof(uuid_t)> binary;
    typedef enum class variant_e : int {
        variant_ncs         = 0,
        variant_dce         = 1,
        variant_microsoft   = 2,
        variant_other       = 3
    } variant_t;
    typedef enum class type_e : int {
        type_dce_nil        = 0,
        type_dce_time       = 1, // time-based UUID with a MAC address
        type_dce_security   = 2,
        type_dce_md5        = 3, // name-based UUID with MD5
        type_dce_random     = 4,
        type_dce_sha1       = 5, // name-based UUID with SHA1
        type_dce_time_v6    = 6, // time-based version 6 UUID (according to RFC 9562) with a MAC address
        type_dce_time_v7    = 7, // time-based version 7 UUID (according to RFC 9652)
        type_dce_vendor     = 8,
        type_dce_other      = 9
    } type_t;

public:
    ~Uuid() = default;
    Uuid();
    Uuid(const Uuid&);
    Uuid(Uuid&&) noexcept = default;
    Uuid& operator=(const Uuid&);
    Uuid& operator=(Uuid&&) noexcept = default;

    explicit Uuid(const uuid_t& raw);
    explicit Uuid(const char* uuid_str);
    explicit Uuid(const std::string& uuid_str);
    explicit Uuid(const binary& data);

    size_t hash() const; // generate a hash value for the UUID for use in unordered_set and unordered_map
    void swap(Uuid& uuid) noexcept;
    void clear();

    bool is_null() const;
    bool try_get_time(struct timeval* tv) const;
    Uuid::variant_t variant() const;
    std::string variant_str() const;
    Uuid::type_t type() const;
    std::string type_str() const;

    bool set(const char* uuid_str);
    bool set(const std::string& uuid_str);

    Uuid::binary to_binary() const;
    void from_binary(const binary& data);

    std::string to_string_lowercase() const;
    std::string to_string_uppercase() const;

    bool operator!=(const Uuid& right) const;
    bool operator==(const Uuid& right) const;
    bool operator<(const Uuid& right) const;
    bool operator>(const Uuid& right) const;

    static Uuid null_based();   // create a null UUID object
    static Uuid time_based();   // create a time UUID object
    static Uuid random_based(); // create a random UUID object
    static Uuid dns_name_based(const std::string& name);  // create a DNS name-based UUID object
    static Uuid oid_name_based(const std::string& name);  // create a OID name-based UUID object
    static Uuid uri_name_based(const std::string& name);  // create a URI name-based UUID object
    static Uuid x500_name_based(const std::string& name); // create a x500 name-based UUID object
    static bool is_valid(const char* uuid_str);
    static bool is_valid(const std::string& uuid_str);

private:
    uuid_t raw_; // libuuid raw data

    static bool make_from_string(const char* uuid_str, uuid_t& result);
    static bool make_from_namespace_md5(const uuid_t* uuid_ns, const std::string& name, uuid_t& result);
};

inline void swap(Uuid& u1, Uuid& u2) noexcept
{
    u1.swap(u2);
}

} // namespace UuidHelpers

// Install the hashing function for Uuid into the stdlib
namespace std {
    template<>
    struct hash<UuidHelpers::Uuid> {
        std::size_t operator()(UuidHelpers::Uuid const& s) const noexcept {
            return s.hash();
        }
    };
}
