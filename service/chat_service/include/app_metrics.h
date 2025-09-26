#pragma once

#include <set>
#include <map>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

class Metrics {
public:
    Metrics()
    :   latency_buckets_{0.05, 0.1, 0.5, 1.0, 2.0, 5.0},
        registry_(std::make_shared<prometheus::Registry>()) {

        auto& total_c = prometheus::BuildCounter()
            // .Name("http_requests_total")
            .Name("chat_requests_total")
            .Help("HTTP total requests counter")
            .Register(*registry_);
        total_requests_dialog_send_      = &total_c.Add({{"endpoint", "/dialog/:id/send"}});
        total_requests_dialog_list_      = &total_c.Add({{"endpoint", "/dialog/:id/list"}});

        auto& failed_c = prometheus::BuildCounter()
            // .Name("http_requests_failed_total")
            .Name("chat_requests_failed_total")
            .Help("HTTP failed requests counter")
            .Register(*registry_);
        failed_requests_dialog_send_      = &failed_c.Add({{"endpoint", "/dialog/:id/send"}});
        failed_requests_dialog_list_      = &failed_c.Add({{"endpoint", "/dialog/:id/list"}});

        auto& latency_h = prometheus::BuildHistogram()
            // .Name("http_request_duration_seconds")
            .Name("chat_requests_duration_seconds")
            .Help("HTTP request latency")
            .Register(*registry_);
        latency_requests_dialog_send_      = &latency_h.Add({{"endpoint", "/dialog/:id/send"}}, latency_buckets_);
        latency_requests_dialog_list_      = &latency_h.Add({{"endpoint", "/dialog/:id/list"}}, latency_buckets_);
    }

    std::shared_ptr<prometheus::Registry> registry() const { return registry_; }

    void count_request_dialog_send()      { total_requests_dialog_send_->Increment(); }
    void count_request_dialog_list()      { total_requests_dialog_list_->Increment(); }

    void count_failed_request_dialog_send()      { failed_requests_dialog_send_->Increment(); }
    void count_failed_request_dialog_list()      { failed_requests_dialog_list_->Increment(); }

    void store_latency_request_dialog_send(double seconds)      { latency_requests_dialog_send_->Observe(seconds); }
    void store_latency_request_dialog_list(double seconds)      { latency_requests_dialog_list_->Observe(seconds); }

private:
    const std::vector<double>             latency_buckets_{};
    std::shared_ptr<prometheus::Registry> registry_{nullptr};

    prometheus::Counter*   total_requests_dialog_send_{nullptr};
    prometheus::Counter*   total_requests_dialog_list_{nullptr};
    prometheus::Counter*   failed_requests_dialog_send_{nullptr};
    prometheus::Counter*   failed_requests_dialog_list_{nullptr};
    prometheus::Histogram* latency_requests_dialog_send_{nullptr};
    prometheus::Histogram* latency_requests_dialog_list_{nullptr};
};
