#pragma once

#include <httplib.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_database_service.h"
#include "app_cache_service.h"
#include "app_auth_service.h"

class FriendService
{
public:
    ~FriendService() = default;
    FriendService() = delete;
    FriendService(const FriendService&) = delete;
    FriendService(FriendService&&) = delete;
    FriendService& operator=(const FriendService&) = delete;
    FriendService& operator=(FriendService&&) = delete;

    explicit FriendService(std::shared_ptr<Logging::Logger> logger,
                           std::shared_ptr<Metrics> metrics,
                           std::shared_ptr<DatabaseService> db,
                           std::shared_ptr<CacheService> cache,
                           std::shared_ptr<AuthService> auth)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_(std::move(db)),
        cache_(std::move(cache)),
        auth_(std::move(auth)) {}

    void register_endpoints(httplib::Server* server);
    bool pre_routing_validation(const httplib::Request& req);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
    std::shared_ptr<CacheService>    cache_{nullptr};
    std::shared_ptr<AuthService>     auth_{nullptr};

    bool friend_set_id_handler(const httplib::Request& req, httplib::Response& res);
    bool friend_delete_id_handler(const httplib::Request& req, httplib::Response& res);
};
