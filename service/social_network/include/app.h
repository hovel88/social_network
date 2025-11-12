#pragma once

#include <memory>
#include <thread>
#include "app_cache_service.h"
#include "app_database_service.h"
#include "app_auth_service.h"
#include "http_user_service.h"
#include "http_friend_service.h"
#include "http_post_service.h"
#include "http_dialog_service.h"
#include "grpc_likes_service.h"

using OnLivenessCheckFunc  = std::function<bool(void)>;
using OnReadinessCheckFunc = std::function<bool(void)>;

class App
{
public:
    ~App();
    App();
    App(const App&) = delete;
    App(App&&) = delete;
    App& operator=(const App&) = delete;
    App& operator=(App&&) = delete;

    void run();

protected:
    static const size_t CACHE_CAPACITY = 256;
    static const int    CACHE_TTL_SEC  = 60;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};

    std::shared_ptr<drogon::HttpAppFramework> http_server_{nullptr};
    std::thread                               http_server_thread_{};
    OnLivenessCheckFunc                       liveness_check_cb_{};
    OnReadinessCheckFunc                      readiness_check_cb_{};

    std::unique_ptr<grpc::Server> grpc_server_{nullptr};
    std::thread                   grpc_server_thread_{};

    std::shared_ptr<grpc::Channel> grpc_channel_{nullptr};

    std::shared_ptr<KafkaProducer> kafka_producer{nullptr};

    std::shared_ptr<CacheService>      service_cache_{nullptr};
    std::shared_ptr<DatabaseService>   service_database_{nullptr};
    std::shared_ptr<AuthService>       service_auth_{nullptr};
    std::unique_ptr<HttpUserService>   service_user_http_{nullptr};
    std::unique_ptr<HttpFriendService> service_friend_http_{nullptr};
    std::unique_ptr<HttpPostService>   service_post_http_{nullptr};
    std::unique_ptr<HttpDialogService> service_dialog_http_{nullptr};
    std::shared_ptr<GrpcLikesService>  service_likes_grpc_{nullptr};

    std::unique_ptr<prometheus::Exposer> exposer_{nullptr};
    std::shared_ptr<Metrics>             metrics_{nullptr};

    void http_start();
    void grpc_start();

    void on_liveness_check(const OnLivenessCheckFunc& cb) { return on_liveness_check(OnLivenessCheckFunc(cb)); }
    void on_liveness_check(OnLivenessCheckFunc&& cb) { liveness_check_cb_ = std::move(cb); }

    void on_readiness_check(const OnReadinessCheckFunc& cb) { return on_readiness_check(OnReadinessCheckFunc(cb)); }
    void on_readiness_check(OnReadinessCheckFunc&& cb) { readiness_check_cb_ = std::move(cb); }
};
