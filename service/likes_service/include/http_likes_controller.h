#pragma once

#include <drogon/drogon.h>

#include "database.h"
#include "likes_saga.h"
#include "kafka_likes_producer.h"
#include "grpc_likes_client.h"

class HttpLikesController : public drogon::HttpController<HttpLikesController, true>
{
public:
    ~HttpLikesController() = default;
    HttpLikesController()
    :   logger_(Configuration::instance().get_logger()),
        db_(Database::instance()),
        saga_(LikesSaga::instance()),
        grpc_client_(GrpcLikesClient::instance()),
        kafka_producer_(KafkaLikesProducer::instance())
    {}
    HttpLikesController(const HttpLikesController&) = default;
    HttpLikesController(HttpLikesController&&) = default;
    HttpLikesController& operator=(const HttpLikesController&) = default;
    HttpLikesController& operator=(HttpLikesController&&) = default;

    static void initPathRouting();

protected:
    void add_like(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id);
    void remove_like(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id);
    void get_likes_count(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id);
    void get_likes_details(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id);

private:
    std::shared_ptr<Logging::Logger>    logger_{nullptr};
    Database&                           db_;
    LikesSaga&                          saga_;
    GrpcLikesClient&                    grpc_client_;
    KafkaLikesProducer&                 kafka_producer_;
};
