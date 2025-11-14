#pragma once

#include <memory>
#include <thread>
#include <drogon/drogon.h>

#include "logger/logger.h"

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

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};

    std::shared_ptr<drogon::HttpAppFramework> http_server_{nullptr};
    std::thread                               http_server_thread_{};
    OnLivenessCheckFunc                       liveness_check_cb_{};
    OnReadinessCheckFunc                      readiness_check_cb_{};

    void http_start();

    void on_liveness_check(const OnLivenessCheckFunc& cb) { return on_liveness_check(OnLivenessCheckFunc(cb)); }
    void on_liveness_check(OnLivenessCheckFunc&& cb) { liveness_check_cb_ = std::move(cb); }

    void on_readiness_check(const OnReadinessCheckFunc& cb) { return on_readiness_check(OnReadinessCheckFunc(cb)); }
    void on_readiness_check(OnReadinessCheckFunc&& cb) { readiness_check_cb_ = std::move(cb); }
};
