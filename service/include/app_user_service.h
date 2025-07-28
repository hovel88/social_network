#pragma once

#include <drogon/drogon.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_database_service.h"

class UserService
{
public:
    ~UserService() = default;
    UserService() = delete;
    UserService(const UserService&) = delete;
    UserService(UserService&&) = delete;
    UserService& operator=(const UserService&) = delete;
    UserService& operator=(UserService&&) = delete;

    explicit UserService(std::shared_ptr<Logging::Logger> logger,
                         std::shared_ptr<Metrics> metrics,
                         std::shared_ptr<DatabaseService> db)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_(std::move(db)) {}

    void register_endpoints(drogon::HttpAppFramework* server);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};

    bool login_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);
    bool user_register_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);
    bool user_get_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool user_search_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);

    static std::string serialize_users(const std::vector<DatabaseService::User>& users);
    static std::string serialize_user(const DatabaseService::User& user);
};
