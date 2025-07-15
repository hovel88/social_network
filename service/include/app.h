#pragma once

#include "app_cache_service.h"
#include "app_database_service.h"
#include "app_auth_service.h"
#include "app_user_service.h"
#include "app_friend_service.h"
#include "app_post_service.h"
#include "app_dialog_service.h"
#include "configuration/configuration.h"
#include "helpers/thread_pool.h"

namespace SocialNetwork {

class ThreadPoolAdaptor : public httplib::TaskQueue
{
public:
    virtual ~ThreadPoolAdaptor() {}
    ThreadPoolAdaptor(const std::string_view name,
                      std::shared_ptr<Logging::Logger> logger,
                      uint64_t threads_count,
                      uint64_t tasks_capacity)
    :   pool_(name, logger, threads_count, tasks_capacity) {}

    virtual bool enqueue(std::function<void()> fn) override final {
        // Return 'true' if the task was actually enqueued,
        // or 'false' if the caller must drop the corresponding connection
        return pool_.add_task(nullptr, fn).has_value();
    }

    virtual void shutdown() override final {
        pool_.wait_all();
    }

private:
    ThreadHelpers::ThreadPool pool_;
};


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

    std::unique_ptr<httplib::Server> http_server_{nullptr};
    std::thread                      http_server_thread_{};
    OnLivenessCheckFunc              liveness_check_cb_{};
    OnReadinessCheckFunc             readiness_check_cb_{};
    std::shared_ptr<CacheService>    service_cache{nullptr};
    std::shared_ptr<DatabaseService> service_database{nullptr};
    std::shared_ptr<AuthService>     service_auth{nullptr};
    std::unique_ptr<UserService>     service_user{nullptr};
    std::unique_ptr<FriendService>   service_friend{nullptr};
    std::unique_ptr<PostService>     service_post{nullptr};
    std::unique_ptr<DialogService>   service_dialog{nullptr};

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

    bool pre_routing_handler(const httplib::Request& req, httplib::Response& res);
    void liveness_handler(const httplib::Request& req, httplib::Response& res);
    void readiness_handler(const httplib::Request& req, httplib::Response& res);
    void post_routing_handler(const httplib::Request& req, httplib::Response& res);
    void error_handler(const httplib::Request& req, httplib::Response& res);
    void exception_handler(const httplib::Request& req, httplib::Response& res, std::exception_ptr ep);
    void log_handler(const httplib::Request& req, const httplib::Response& res);
};

} // namespace SocialNetwork
