#pragma once

#include <queue>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <functional>
#include <any>
#include "logger/logger.h"

namespace SocialNetwork {

namespace ThreadHelpers {

class PooledTask
{
public:
    ~PooledTask() = default;
    PooledTask() = delete;
    PooledTask(const PooledTask&) = default;
    PooledTask(PooledTask&&) = default;
    PooledTask& operator=(const PooledTask&) = default;
    PooledTask& operator=(PooledTask&&) = default;

    template<typename Func, typename... Args>
    explicit PooledTask(const Func& func, Args&&... args)
    :   is_void{std::is_void_v<decltype(func(std::forward<Args>(args)...))>} {
        // using R = decltype(func(std::forward<Args>(args)...));
        if constexpr (std::is_void_v<decltype(func(std::forward<Args>(args)...))>) {
            void_func = std::bind(func, std::forward<Args>(args)...);
            any_func  = []()->int { return 0; };
        } else {
            void_func = []()->void {};
            any_func  = std::bind(func, std::forward<Args>(args)...);
        }
    }

    void operator() () {
        void_func();
        result = any_func();
    }

    std::any get_result() const {
        if (is_void) return {};
        return result;
    }

private:
    const bool                  is_void{false};
    std::function<void()>       void_func{nullptr};
    std::function<std::any()>   any_func{nullptr};
    std::any                    result{};
};



using OnCompleteFunc = std::function<void(std::any)>;

class ThreadPool
{
protected:
    struct TaskMeta {
        TaskMeta(uint64_t id, OnCompleteFunc& cb)
        :   id_(id), cb_(cb) {}

        uint64_t       id_{0};
        OnCompleteFunc cb_{};
    };

public:
    ~ThreadPool();
    ThreadPool() = delete;
    ThreadPool(const ThreadPool&) = default;
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(const ThreadPool&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;

    explicit ThreadPool(const std::string_view name,
                        std::shared_ptr<Logging::Logger> logger,
                        uint64_t threads_count,
                        uint64_t tasks_capacity);

    template<typename Func, typename... Args>
    std::optional<uint64_t> add_task(OnCompleteFunc cb, Func func, Args&&... args)
    {
        uint64_t id = 0;
        bool refused = false;

        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            id = statistic_.tasks_last_id++;
            if (tasks_.size() < tasks_max_capacity_) {
                auto& t = tasks_.emplace(PooledTask(func, std::forward<Args>(args)...), TaskMeta(id, cb));
                tasks_condition_.notify_one();
            } else {
                refused = true;
                ++statistic_.tasks_refused_count;
            }
        }
        if (refused && cb) {
            cb({});
            return std::nullopt;
        }

        return id;
    }

    void wait_all();

    // <statistic>
    uint64_t threads_max_count() const { return threads_max_count_; }
    uint64_t tasks_max_capacity() const { return tasks_max_capacity_; }

    uint64_t tasks_total_count() const { return statistic_.tasks_last_id; }
    uint64_t tasks_completed_count() const { return statistic_.tasks_completed_count; }
    uint64_t tasks_refused_count() const { return statistic_.tasks_refused_count; }
    uint64_t tasks_active_count() const { return tasks_.size(); }
    // </statistic>

private:
    std::shared_ptr<Logging::Logger> logger_{};
    const uint64_t threads_max_count_{0};
    const uint64_t tasks_max_capacity_{0};

    struct statistic_s {
        std::atomic<uint64_t> tasks_last_id{0};
        std::atomic<uint64_t> tasks_completed_count{0};
        std::atomic<uint64_t> tasks_refused_count{0};
    } statistic_{};

    std::atomic<bool>        threads_stop_{false};
    std::vector<std::thread> threads_{};

    std::mutex                                  tasks_mutex_{};
    std::condition_variable                     tasks_condition_{};
    std::queue<std::pair<PooledTask, TaskMeta>> tasks_{};

    std::mutex              task_results_all_mutex_{};
    std::condition_variable task_results_all_condition_{};

    void run(const std::string thread_name);
};

} // namespace ThreadHelpers

} // namespace SocialNetwork
