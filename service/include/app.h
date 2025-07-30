#pragma once

#include "app_cache_service.h"
#include "app_database_service.h"
#include "app_auth_service.h"
#include "app_user_service.h"
#include "app_friend_service.h"
#include "app_post_service.h"
#include "app_dialog_service.h"
#include "app_inmem_service.h"
#include "configuration/configuration.h"
#include "helpers/thread_pool.h"

using OnLivenessCheckFunc  = std::function<bool(void)>;
using OnReadinessCheckFunc = std::function<bool(void)>;

class App
{
public:
    ~App();
    App() = delete;
    App(const App&) = delete;
    App(App&&) = delete;
    App& operator=(const App&) = delete;
    App& operator=(App&&) = delete;

    explicit App(std::shared_ptr<cxxopts::ParseResult> cli_opts);

    void run();

protected:
    static const size_t CACHE_CAPACITY = 256;
    static const int    CACHE_TTL_SEC  = 60;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Configuration>   conf_{nullptr};

    std::shared_ptr<drogon::HttpAppFramework> http_server_{nullptr};
    std::thread                               http_server_thread_{};
    OnLivenessCheckFunc                       liveness_check_cb_{};
    OnReadinessCheckFunc                      readiness_check_cb_{};

    std::shared_ptr<CacheService>    service_cache_{nullptr};
    std::shared_ptr<DatabaseService> service_database_{nullptr};
    std::shared_ptr<InMemService>    service_inmem_{nullptr};
    std::shared_ptr<AuthService>     service_auth_{nullptr};
    std::unique_ptr<UserService>     service_user_{nullptr};
    std::unique_ptr<FriendService>   service_friend_{nullptr};
    std::unique_ptr<PostService>     service_post_{nullptr};
    std::unique_ptr<DialogService>   service_dialog_{nullptr};

    std::unique_ptr<prometheus::Exposer> exposer_{nullptr};
    std::shared_ptr<Metrics>             metrics_{nullptr};
    std::shared_ptr<ConnectionPool>      db_pool_{nullptr};
    std::set<std::string>                db_host_tags{};

    void db_start();
    void http_start();

    void on_liveness_check(const OnLivenessCheckFunc& cb) { return on_liveness_check(OnLivenessCheckFunc(cb)); }
    void on_liveness_check(OnLivenessCheckFunc&& cb) { liveness_check_cb_ = std::move(cb); }

    void on_readiness_check(const OnReadinessCheckFunc& cb) { return on_readiness_check(OnReadinessCheckFunc(cb)); }
    void on_readiness_check(OnReadinessCheckFunc&& cb) { readiness_check_cb_ = std::move(cb); }

    //void log_handler(const httplib::Request& req, const httplib::Response& res);
};
