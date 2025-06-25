#pragma once

#include <queue>
#include <mutex>
#include <pqxx/pqxx>
#include <httplib.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "logger/logger.h"
#include "configuration/configuration.h"
#include "helpers/thread_pool.h"

namespace SocialNetwork {

class Metrics {
public:
    Metrics()
    :   latency_buckets_{0.05, 0.1, 0.5, 1.0, 2.0, 5.0},
        registry_(std::make_shared<prometheus::Registry>()) {

        auto& total_c = prometheus::BuildCounter()
            .Name("http_requests_total")
            .Help("HTTP total requests counter")
            .Register(*registry_);
        total_requests_login_         = &total_c.Add({{"endpoint", "/login"}});
        total_requests_user_register_ = &total_c.Add({{"endpoint", "/user/register"}});
        total_requests_user_get_id_   = &total_c.Add({{"endpoint", "/user/get/:id"}});
        total_requests_user_search_   = &total_c.Add({{"endpoint", "/user/search"}});

        auto& failed_c = prometheus::BuildCounter()
            .Name("http_requests_failed_total")
            .Help("HTTP failed requests counter")
            .Register(*registry_);
        failed_requests_login_         = &failed_c.Add({{"endpoint", "/login"}});
        failed_requests_user_register_ = &failed_c.Add({{"endpoint", "/user/register"}});
        failed_requests_user_get_id_   = &failed_c.Add({{"endpoint", "/user/get/:id"}});
        failed_requests_user_search_   = &failed_c.Add({{"endpoint", "/user/search"}});

        auto& latency_h = prometheus::BuildHistogram()
            .Name("http_request_duration_seconds")
            .Help("HTTP request latency")
            .Register(*registry_);
        latency_requests_login_         = &latency_h.Add({{"endpoint", "/login"}}, latency_buckets_);
        latency_requests_user_register_ = &latency_h.Add({{"endpoint", "/user/register"}}, latency_buckets_);
        latency_requests_user_get_id_   = &latency_h.Add({{"endpoint", "/user/get/:id"}}, latency_buckets_);
        latency_requests_user_search_   = &latency_h.Add({{"endpoint", "/user/search"}}, latency_buckets_);
    }

    std::shared_ptr<prometheus::Registry> registry() const { return registry_; }

    void count_request_login()         { total_requests_login_->Increment(); }
    void count_request_user_register() { total_requests_user_register_->Increment(); }
    void count_request_user_get_id()   { total_requests_user_get_id_->Increment(); }
    void count_request_user_search()   { total_requests_user_search_->Increment(); }

    void count_failed_request_login()         { failed_requests_login_->Increment(); }
    void count_failed_request_user_register() { failed_requests_user_register_->Increment(); }
    void count_failed_request_user_get_id()   { failed_requests_user_get_id_->Increment(); }
    void count_failed_request_user_search()   { failed_requests_user_search_->Increment(); }

    void store_latency_request_login(double seconds)         { latency_requests_login_->Observe(seconds); }
    void store_latency_request_user_register(double seconds) { latency_requests_user_register_->Observe(seconds); }
    void store_latency_request_user_get_id(double seconds)   { latency_requests_user_get_id_->Observe(seconds); }
    void store_latency_request_user_search(double seconds)   { latency_requests_user_search_->Observe(seconds); }

private:
    const std::vector<double>             latency_buckets_{};
    std::shared_ptr<prometheus::Registry> registry_{nullptr};
    prometheus::Counter*                  total_requests_login_{nullptr};
    prometheus::Counter*                  total_requests_user_register_{nullptr};
    prometheus::Counter*                  total_requests_user_get_id_{nullptr};
    prometheus::Counter*                  total_requests_user_search_{nullptr};
    prometheus::Counter*                  failed_requests_login_{nullptr};
    prometheus::Counter*                  failed_requests_user_register_{nullptr};
    prometheus::Counter*                  failed_requests_user_get_id_{nullptr};
    prometheus::Counter*                  failed_requests_user_search_{nullptr};
    prometheus::Histogram*                latency_requests_login_{nullptr};
    prometheus::Histogram*                latency_requests_user_register_{nullptr};
    prometheus::Histogram*                latency_requests_user_get_id_{nullptr};
    prometheus::Histogram*                latency_requests_user_search_{nullptr};
};

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

class ConnectionPool
{
public:
    ConnectionPool(const std::string& conn_str, size_t pool_size)
    :   conn_str_(conn_str) {
        for (size_t i = 0; i < pool_size; ++i) {
            pool_.push(std::make_unique<pqxx::connection>(conn_str_));
        }
    }

    std::unique_ptr<pqxx::connection> get_connection() {
        std::lock_guard<std::mutex> lock(pool_mtx_);
        if (pool_.empty()) {
            throw std::runtime_error("No connections available");
        }
        auto conn = std::move(pool_.front());
        pool_.pop();
        if (!conn->is_open()) {
            conn = std::make_unique<pqxx::connection>(conn_str_);
        }
        return conn;
    }

    void release_connection(std::unique_ptr<pqxx::connection>& conn) {
        std::lock_guard<std::mutex> lock(pool_mtx_);
        pool_.push(std::move(conn));
    }

private:
    const std::string                             conn_str_{};
    std::mutex                                    pool_mtx_{};
    std::queue<std::unique_ptr<pqxx::connection>> pool_{};
};

struct ScopedConnection
{
    std::shared_ptr<ConnectionPool>   pool{nullptr};
    std::unique_ptr<pqxx::connection> conn{nullptr};

    ~ScopedConnection() { pool->release_connection(conn); }
    ScopedConnection(std::shared_ptr<ConnectionPool>& p)
    :   pool(p), conn(p->get_connection()) {}
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

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Configuration>   conf_{nullptr};
    std::vector<std::string>         indexes_to_drop_{};
    std::vector<std::string>         indexes_to_create_{};

    std::unique_ptr<httplib::Server> http_server_{nullptr};
    OnLivenessCheckFunc              liveness_check_cb_{};
    OnReadinessCheckFunc             readiness_check_cb_{};
    std::thread                      http_server_thread_{};

    std::unique_ptr<prometheus::Exposer> exposer_{nullptr};
    std::shared_ptr<Metrics>             metrics_{nullptr};

    std::shared_ptr<ConnectionPool> db_pool_{nullptr};
    std::thread                     db_client_thread_{};

    void db_start();
    void http_start();

    void on_liveness_check(const OnLivenessCheckFunc& cb) { return on_liveness_check(OnLivenessCheckFunc(cb)); }
    void on_liveness_check(OnLivenessCheckFunc&& cb) { liveness_check_cb_ = std::move(cb); }

    void on_readiness_check(const OnReadinessCheckFunc& cb) { return on_readiness_check(OnReadinessCheckFunc(cb)); }
    void on_readiness_check(OnReadinessCheckFunc&& cb) { readiness_check_cb_ = std::move(cb); }

    bool pre_routing_handler(const httplib::Request& req, httplib::Response& res);
    bool login_handler(const httplib::Request& req, httplib::Response& res);
    bool user_register_handler(const httplib::Request& req, httplib::Response& res);
    bool user_get_id_handler(const httplib::Request& req, httplib::Response& res);
    bool user_search_handler(const httplib::Request& req, httplib::Response& res);
    void liveness_handler(const httplib::Request& req, httplib::Response& res);
    void readiness_handler(const httplib::Request& req, httplib::Response& res);
    void post_routing_handler(const httplib::Request& req, httplib::Response& res);
    void error_handler(const httplib::Request& req, httplib::Response& res);
    void exception_handler(const httplib::Request& req, httplib::Response& res, std::exception_ptr ep);
    void log_handler(const httplib::Request& req, const httplib::Response& res);

    void db_create_users_table();
    void db_create_index_users_names_search();
    void db_drop_index_users_names_search();
};

} // namespace SocialNetwork
