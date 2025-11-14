#pragma once

#include <regex>

#include "database.h"
#include "configuration.h"

class Auth
{
public:
    ~Auth() = default;
    Auth(const Auth&) = delete;
    Auth(Auth&&) = delete;
    Auth& operator=(const Auth&) = delete;
    Auth& operator=(Auth&&) = delete;

    static Auth& instance()
    {
        static Auth singleton;
        return singleton;
    }

    bool authenticate(std::string& user_id);
    static bool is_valid_uuid(const std::string& id) {
        static const std::regex uuid_regex(
            "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
        );
        return std::regex_match(id, uuid_regex);
    }

private:
    Auth()
    :   logger_(Configuration::instance().get_logger()),
        db_(Database::instance())
    {}

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    Database&                        db_;
};
