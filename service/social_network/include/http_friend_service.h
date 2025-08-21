#pragma once

#include <drogon/drogon.h>
#include "app_metrics.h"
#include "app_database_service.h"
#include "app_cache_service.h"
#include "configuration.h"

class HttpFriendService
{
public:
    ~HttpFriendService() = default;
    HttpFriendService() = delete;
    HttpFriendService(const HttpFriendService&) = delete;
    HttpFriendService(HttpFriendService&&) = delete;
    HttpFriendService& operator=(const HttpFriendService&) = delete;
    HttpFriendService& operator=(HttpFriendService&&) = delete;

    explicit HttpFriendService(std::shared_ptr<Metrics> metrics,
                               std::shared_ptr<DatabaseService> db,
                               std::shared_ptr<CacheService> cache)
    :   logger_(Configuration::instance().get_logger()),
        metrics_(std::move(metrics)),
        db_(std::move(db)),
        cache_(std::move(cache)) {}

    void register_endpoints(drogon::HttpAppFramework* server);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
    std::shared_ptr<CacheService>    cache_{nullptr};

    bool friend_set_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool friend_delete_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
};
