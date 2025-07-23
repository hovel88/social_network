#pragma once

#include <httplib.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_connection_pool.h"
#include "app_database_service.h"
#include "app_cache_service.h"
#include "app_auth_service.h"

class PostService
{
public:
    ~PostService() = default;
    PostService() = delete;
    PostService(const PostService&) = delete;
    PostService(PostService&&) = delete;
    PostService& operator=(const PostService&) = delete;
    PostService& operator=(PostService&&) = delete;

    explicit PostService(std::shared_ptr<Logging::Logger> logger,
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

    bool post_get_id_handler(const httplib::Request& req, httplib::Response& res);
    bool post_delete_id_handler(const httplib::Request& req, httplib::Response& res);
    bool post_create_handler(const httplib::Request& req, httplib::Response& res);
    bool post_update_handler(const httplib::Request& req, httplib::Response& res);
    bool post_feed_handler(const httplib::Request& req, httplib::Response& res);

    static std::vector<DatabaseService::Post> get_page(const std::vector<DatabaseService::Post>& feed, size_t offset, size_t limit);
    static std::string serialize_posts(const std::vector<DatabaseService::Post>& posts);
    static std::string serialize_post(const DatabaseService::Post& post);
};
