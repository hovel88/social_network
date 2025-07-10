#pragma once

#include <optional>
#include <vector>
#include <string>
#include <chrono>
#include <cache.hpp>
#include <lru_cache_policy.hpp>
#include "app_database_service.h"

namespace SocialNetwork {

// alias for an easy class typing
template <typename Key, typename Value>
using lru_cache_t = typename caches::fixed_sized_cache<Key, Value, caches::LRUCachePolicy>;

class CacheService
{
protected:
    static const size_t NUMBER_OF_CACHED_POSTS = 1000;

    struct CacheItem {
        std::vector<DatabaseService::Post>    feed{};
        std::chrono::system_clock::time_point expires_at{};
    };

public:
    ~CacheService() = default;
    CacheService() = delete;
    CacheService(const CacheService&) = delete;
    CacheService(CacheService&&) = delete;
    CacheService& operator=(const CacheService&) = delete;
    CacheService& operator=(CacheService&&) = delete;

    explicit CacheService(size_t capacity,
                          std::chrono::seconds ttl = std::chrono::minutes(5))
    :   cache_(capacity),
        ttl_(ttl) {}

    std::optional<std::vector<DatabaseService::Post>> get_feed(const std::string& user_id, std::chrono::system_clock::time_point& expires_at, std::chrono::seconds& remaining_ttl) {
        auto item = cache_.TryGet(user_id);
        if (item.second) {
            auto now = std::chrono::system_clock::now();
            if (item.first->expires_at > now) {
                expires_at    = item.first->expires_at;
                remaining_ttl = std::chrono::duration_cast<std::chrono::seconds>(expires_at - now);
                return item.first->feed;
            }
            // удаляем протухший кеш
            cache_.Remove(user_id);
        }
        return std::nullopt;
    }

    void put_feed(const std::string& user_id, const std::vector<DatabaseService::Post>& feed) {
        CacheItem item{
            .feed       = feed,
            .expires_at = std::chrono::system_clock::now() + ttl_
        };
        cache_.Put(user_id, item);
    }

    void invalidate_user(const std::string& user_id) {
        cache_.Remove(user_id);
    }

    void add_post_to_feed(const std::string& user_id, const DatabaseService::Post& post) {
        auto item = cache_.TryGet(user_id);
        if (!item.second) return;

        // XXX: сделать проверку протухания кеша и удалить?

        auto it = std::lower_bound(item.first->feed.begin(), item.first->feed.end(), post, 
            [](const DatabaseService::Post& a, const DatabaseService::Post& b) {
                // сортируем посты по убыванию времени
                return a.created_at_msec > b.created_at_msec;
            });
        item.first->feed.insert(it, post);

        if (item.first->feed.size() > NUMBER_OF_CACHED_POSTS) {
            item.first->feed.resize(NUMBER_OF_CACHED_POSTS);
        }
        cache_.Put(user_id, *item.first);
    }

private:
    //     key = UUID (user_id),
    //   value = <vector of [Post], expires_at>
    lru_cache_t<std::string, CacheItem> cache_;
    const std::chrono::seconds          ttl_{};
};

} // namespace SocialNetwork
