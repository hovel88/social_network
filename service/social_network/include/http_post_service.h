#pragma once

#include <drogon/drogon.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_connection_pool.h"
#include "app_database_service.h"
#include "app_cache_service.h"
#include "kafka_client_producer.h"

class HttpPostService
{
public:
    ~HttpPostService() = default;
    HttpPostService() = delete;
    HttpPostService(const HttpPostService&) = delete;
    HttpPostService(HttpPostService&&) = delete;
    HttpPostService& operator=(const HttpPostService&) = delete;
    HttpPostService& operator=(HttpPostService&&) = delete;

    explicit HttpPostService(std::shared_ptr<Logging::Logger> logger,
                             std::shared_ptr<Metrics> metrics,
                             std::shared_ptr<DatabaseService> db,
                             std::shared_ptr<CacheService> cache,
                             std::shared_ptr<KafkaProducer> kafka_producer)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_(std::move(db)),
        cache_(std::move(cache)),
        kafka_producer_(std::move(kafka_producer)) {}

    void register_endpoints(drogon::HttpAppFramework* server);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
    std::shared_ptr<CacheService>    cache_{nullptr};
    std::shared_ptr<KafkaProducer>   kafka_producer_{nullptr};

    bool post_get_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool post_delete_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool post_create_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);
    bool post_update_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);
    bool post_feed_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res);

    static std::vector<DatabaseService::Post> get_page(const std::vector<DatabaseService::Post>& feed, size_t offset, size_t limit);
    static std::string serialize_posts(const std::vector<DatabaseService::Post>& posts);
    static std::string serialize_post(const DatabaseService::Post& post);
};
