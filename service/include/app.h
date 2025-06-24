#pragma once

#include <queue>
#include <mutex>
#include <pqxx/pqxx>
#include <httplib.h>
#include "logger/logger.h"
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

    std::unique_ptr<httplib::Server> http_server_{nullptr};
    OnLivenessCheckFunc              liveness_check_cb_{};
    OnReadinessCheckFunc             readiness_check_cb_{};
    std::thread                      http_server_thread_{};

    std::shared_ptr<ConnectionPool> db_pool_{nullptr};
    std::thread                     db_client_thread_{};

    void db_start();
    void http_start();

    void on_liveness_check(const OnLivenessCheckFunc& cb) { return on_liveness_check(OnLivenessCheckFunc(cb)); }
    void on_liveness_check(OnLivenessCheckFunc&& cb) { liveness_check_cb_ = std::move(cb); }

    void on_readiness_check(const OnReadinessCheckFunc& cb) { return on_readiness_check(OnReadinessCheckFunc(cb)); }
    void on_readiness_check(OnReadinessCheckFunc&& cb) { readiness_check_cb_ = std::move(cb); }

    bool pre_routing_handler(const httplib::Request& req, httplib::Response& res);
    void login_handler(const httplib::Request& req, httplib::Response& res);
    void user_register_handler(const httplib::Request& req, httplib::Response& res);
    void user_get_id_handler(const httplib::Request& req, httplib::Response& res);
    void liveness_handler(const httplib::Request& req, httplib::Response& res);
    void readiness_handler(const httplib::Request& req, httplib::Response& res);
    void post_routing_handler(const httplib::Request& req, httplib::Response& res);
    void error_handler(const httplib::Request& req, httplib::Response& res);
    void exception_handler(const httplib::Request& req, httplib::Response& res, std::exception_ptr ep);
    void log_handler(const httplib::Request& req, const httplib::Response& res);

    void db_create_users_table();
};

} // namespace SocialNetwork
