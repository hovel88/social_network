#pragma once

#include <drogon/drogon.h>
#include <regex>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_database_service.h"

class AuthService
{
public:
    ~AuthService() = default;
    AuthService() = delete;
    AuthService(const AuthService&) = delete;
    AuthService(AuthService&&) = delete;
    AuthService& operator=(const AuthService&) = delete;
    AuthService& operator=(AuthService&&) = delete;

    explicit AuthService(std::shared_ptr<Logging::Logger> logger,
                         std::shared_ptr<Metrics> metrics,
                         std::shared_ptr<DatabaseService> db)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_(std::move(db)) {}

    bool authenticate(const drogon::HttpRequestPtr& req, std::string& user_id);
    static bool is_valid_uuid(const std::string& id) {
        static const std::regex uuid_regex(
            "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
        );
        return std::regex_match(id, uuid_regex);
    }

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
};
