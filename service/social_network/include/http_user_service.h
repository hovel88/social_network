#pragma once

#include <drogon/drogon.h>
#include "app_metrics.h"
#include "app_database_service.h"
#include "configuration.h"

class HttpUserService
{
public:
    ~HttpUserService() = default;
    HttpUserService() = delete;
    HttpUserService(const HttpUserService&) = delete;
    HttpUserService(HttpUserService&&) = delete;
    HttpUserService& operator=(const HttpUserService&) = delete;
    HttpUserService& operator=(HttpUserService&&) = delete;

    explicit HttpUserService(std::shared_ptr<Metrics> metrics,
                             std::shared_ptr<DatabaseService> db)
    :   logger_(Configuration::instance().get_logger()),
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
