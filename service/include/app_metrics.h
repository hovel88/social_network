#pragma once

#include <set>
#include <map>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

namespace SocialNetwork {

class Metrics {
public:
    Metrics(const std::set<std::string>& tags)
    :   latency_buckets_{0.05, 0.1, 0.5, 1.0, 2.0, 5.0},
        registry_(std::make_shared<prometheus::Registry>()) {

        auto& host_c = prometheus::BuildCounter()
            .Name("http_requests_to_host_total")
            .Help("HTTP total requests to specific host counter")
            .Register(*registry_);
        for (const auto t : tags) {
            total_requests_to_host_.insert(std::make_pair(t, &host_c.Add({{"host", t}})));
        }

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

    void count_request_to_host(const std::string& tag) {
        auto counter = total_requests_to_host_.find(tag);
        if (counter != total_requests_to_host_.end()) {
            counter->second->Increment();
        }
    }

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

    std::map<std::string, prometheus::Counter*> total_requests_to_host_{};

    prometheus::Counter*   total_requests_login_{nullptr};
    prometheus::Counter*   total_requests_user_register_{nullptr};
    prometheus::Counter*   total_requests_user_get_id_{nullptr};
    prometheus::Counter*   total_requests_user_search_{nullptr};
    prometheus::Counter*   failed_requests_login_{nullptr};
    prometheus::Counter*   failed_requests_user_register_{nullptr};
    prometheus::Counter*   failed_requests_user_get_id_{nullptr};
    prometheus::Counter*   failed_requests_user_search_{nullptr};
    prometheus::Histogram* latency_requests_login_{nullptr};
    prometheus::Histogram* latency_requests_user_register_{nullptr};
    prometheus::Histogram* latency_requests_user_get_id_{nullptr};
    prometheus::Histogram* latency_requests_user_search_{nullptr};
};

} // namespace SocialNetwork
