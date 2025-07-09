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
        total_requests_login_            = &total_c.Add({{"endpoint", "/login"}});
        total_requests_user_register_    = &total_c.Add({{"endpoint", "/user/register"}});
        total_requests_user_get_id_      = &total_c.Add({{"endpoint", "/user/get/:id"}});
        total_requests_user_search_      = &total_c.Add({{"endpoint", "/user/search"}});
        total_requests_friend_set_id_    = &total_c.Add({{"endpoint", "/friend/set/:id"}});
        total_requests_friend_delete_id_ = &total_c.Add({{"endpoint", "/friend/delete/:id"}});
        total_requests_post_get_id_      = &total_c.Add({{"endpoint", "/post/get/:id"}});
        total_requests_post_delete_id_   = &total_c.Add({{"endpoint", "/post/delete/:id"}});
        total_requests_post_create_      = &total_c.Add({{"endpoint", "/post/create"}});
        total_requests_post_update_      = &total_c.Add({{"endpoint", "/post/update"}});
        total_requests_post_feed_        = &total_c.Add({{"endpoint", "/post/feed"}});

        auto& failed_c = prometheus::BuildCounter()
            .Name("http_requests_failed_total")
            .Help("HTTP failed requests counter")
            .Register(*registry_);
        failed_requests_login_            = &failed_c.Add({{"endpoint", "/login"}});
        failed_requests_user_register_    = &failed_c.Add({{"endpoint", "/user/register"}});
        failed_requests_user_get_id_      = &failed_c.Add({{"endpoint", "/user/get/:id"}});
        failed_requests_user_search_      = &failed_c.Add({{"endpoint", "/user/search"}});
        failed_requests_friend_set_id_    = &failed_c.Add({{"endpoint", "/friend/set/:id"}});
        failed_requests_friend_delete_id_ = &failed_c.Add({{"endpoint", "/friend/delete/:id"}});
        failed_requests_post_get_id_      = &failed_c.Add({{"endpoint", "/post/get/:id"}});
        failed_requests_post_delete_id_   = &failed_c.Add({{"endpoint", "/post/delete/:id"}});
        failed_requests_post_create_      = &failed_c.Add({{"endpoint", "/post/create"}});
        failed_requests_post_update_      = &failed_c.Add({{"endpoint", "/post/update"}});
        failed_requests_post_feed_        = &failed_c.Add({{"endpoint", "/post/feed"}});

        auto& latency_h = prometheus::BuildHistogram()
            .Name("http_request_duration_seconds")
            .Help("HTTP request latency")
            .Register(*registry_);
        latency_requests_login_            = &latency_h.Add({{"endpoint", "/login"}}, latency_buckets_);
        latency_requests_user_register_    = &latency_h.Add({{"endpoint", "/user/register"}}, latency_buckets_);
        latency_requests_user_get_id_      = &latency_h.Add({{"endpoint", "/user/get/:id"}}, latency_buckets_);
        latency_requests_user_search_      = &latency_h.Add({{"endpoint", "/user/search"}}, latency_buckets_);
        latency_requests_friend_set_id_    = &latency_h.Add({{"endpoint", "/friend/set/:id"}}, latency_buckets_);
        latency_requests_friend_delete_id_ = &latency_h.Add({{"endpoint", "/friend/delete/:id"}}, latency_buckets_);
        latency_requests_post_get_id_      = &latency_h.Add({{"endpoint", "/post/get/:id"}}, latency_buckets_);
        latency_requests_post_delete_id_   = &latency_h.Add({{"endpoint", "/post/delete/:id"}}, latency_buckets_);
        latency_requests_post_create_      = &latency_h.Add({{"endpoint", "/post/create"}}, latency_buckets_);
        latency_requests_post_update_      = &latency_h.Add({{"endpoint", "/post/update"}}, latency_buckets_);
        latency_requests_post_feed_        = &latency_h.Add({{"endpoint", "/post/feed"}}, latency_buckets_);
    }

    std::shared_ptr<prometheus::Registry> registry() const { return registry_; }

    void count_request_to_host(const std::string& tag) {
        auto counter = total_requests_to_host_.find(tag);
        if (counter != total_requests_to_host_.end()) {
            counter->second->Increment();
        }
    }

    void count_request_login()            { total_requests_login_->Increment(); }
    void count_request_user_register()    { total_requests_user_register_->Increment(); }
    void count_request_user_get_id()      { total_requests_user_get_id_->Increment(); }
    void count_request_user_search()      { total_requests_user_search_->Increment(); }
    void count_request_friend_set_id()    { total_requests_friend_set_id_->Increment(); }
    void count_request_friend_delete_id() { total_requests_friend_delete_id_->Increment(); }
    void count_request_post_get_id()      { total_requests_post_get_id_->Increment(); }
    void count_request_post_delete_id()   { total_requests_post_delete_id_->Increment(); }
    void count_request_post_create()      { total_requests_post_create_->Increment(); }
    void count_request_post_update()      { total_requests_post_update_->Increment(); }
    void count_request_post_feed()        { total_requests_post_feed_->Increment(); }

    void count_failed_request_login()            { failed_requests_login_->Increment(); }
    void count_failed_request_user_register()    { failed_requests_user_register_->Increment(); }
    void count_failed_request_user_get_id()      { failed_requests_user_get_id_->Increment(); }
    void count_failed_request_user_search()      { failed_requests_user_search_->Increment(); }
    void count_failed_request_friend_set_id()    { failed_requests_friend_set_id_->Increment(); }
    void count_failed_request_friend_delete_id() { failed_requests_friend_delete_id_->Increment(); }
    void count_failed_request_post_get_id()      { failed_requests_post_get_id_->Increment(); }
    void count_failed_request_post_delete_id()   { failed_requests_post_delete_id_->Increment(); }
    void count_failed_request_post_create()      { failed_requests_post_create_->Increment(); }
    void count_failed_request_post_update()      { failed_requests_post_update_->Increment(); }
    void count_failed_request_post_feed()        { failed_requests_post_feed_->Increment(); }

    void store_latency_request_login(double seconds)            { latency_requests_login_->Observe(seconds); }
    void store_latency_request_user_register(double seconds)    { latency_requests_user_register_->Observe(seconds); }
    void store_latency_request_user_get_id(double seconds)      { latency_requests_user_get_id_->Observe(seconds); }
    void store_latency_request_user_search(double seconds)      { latency_requests_user_search_->Observe(seconds); }
    void store_latency_request_friend_set_id(double seconds)    { latency_requests_friend_set_id_->Observe(seconds); }
    void store_latency_request_friend_delete_id(double seconds) { latency_requests_friend_delete_id_->Observe(seconds); }
    void store_latency_request_post_get_id(double seconds)      { latency_requests_post_get_id_->Observe(seconds); }
    void store_latency_request_post_delete_id(double seconds)   { latency_requests_post_delete_id_->Observe(seconds); }
    void store_latency_request_post_create(double seconds)      { latency_requests_post_create_->Observe(seconds); }
    void store_latency_request_post_update(double seconds)      { latency_requests_post_update_->Observe(seconds); }
    void store_latency_request_post_feed(double seconds)        { latency_requests_post_feed_->Observe(seconds); }

private:
    const std::vector<double>             latency_buckets_{};
    std::shared_ptr<prometheus::Registry> registry_{nullptr};

    std::map<std::string, prometheus::Counter*> total_requests_to_host_{};

    prometheus::Counter*   total_requests_login_{nullptr};
    prometheus::Counter*   total_requests_user_register_{nullptr};
    prometheus::Counter*   total_requests_user_get_id_{nullptr};
    prometheus::Counter*   total_requests_user_search_{nullptr};
    prometheus::Counter*   total_requests_friend_set_id_{nullptr};
    prometheus::Counter*   total_requests_friend_delete_id_{nullptr};
    prometheus::Counter*   total_requests_post_get_id_{nullptr};
    prometheus::Counter*   total_requests_post_delete_id_{nullptr};
    prometheus::Counter*   total_requests_post_create_{nullptr};
    prometheus::Counter*   total_requests_post_update_{nullptr};
    prometheus::Counter*   total_requests_post_feed_{nullptr};
    prometheus::Counter*   failed_requests_login_{nullptr};
    prometheus::Counter*   failed_requests_user_register_{nullptr};
    prometheus::Counter*   failed_requests_user_get_id_{nullptr};
    prometheus::Counter*   failed_requests_user_search_{nullptr};
    prometheus::Counter*   failed_requests_friend_set_id_{nullptr};
    prometheus::Counter*   failed_requests_friend_delete_id_{nullptr};
    prometheus::Counter*   failed_requests_post_get_id_{nullptr};
    prometheus::Counter*   failed_requests_post_delete_id_{nullptr};
    prometheus::Counter*   failed_requests_post_create_{nullptr};
    prometheus::Counter*   failed_requests_post_update_{nullptr};
    prometheus::Counter*   failed_requests_post_feed_{nullptr};
    prometheus::Histogram* latency_requests_login_{nullptr};
    prometheus::Histogram* latency_requests_user_register_{nullptr};
    prometheus::Histogram* latency_requests_user_get_id_{nullptr};
    prometheus::Histogram* latency_requests_user_search_{nullptr};
    prometheus::Histogram* latency_requests_friend_set_id_{nullptr};
    prometheus::Histogram* latency_requests_friend_delete_id_{nullptr};
    prometheus::Histogram* latency_requests_post_get_id_{nullptr};
    prometheus::Histogram* latency_requests_post_delete_id_{nullptr};
    prometheus::Histogram* latency_requests_post_create_{nullptr};
    prometheus::Histogram* latency_requests_post_update_{nullptr};
    prometheus::Histogram* latency_requests_post_feed_{nullptr};
};

} // namespace SocialNetwork
