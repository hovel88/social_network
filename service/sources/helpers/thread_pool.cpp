#include <format>
#include "helpers/thread.h"
#include "helpers/thread_pool.h"

namespace SocialNetwork {

namespace ThreadHelpers {

ThreadPool::~ThreadPool()
{
    threads_stop_ = true;
    tasks_condition_.notify_all();
    for (auto& th : threads_) {
        th.join();
    }
}

ThreadPool::ThreadPool(const std::string_view name,
                       std::shared_ptr<Logging::Logger> logger,
                       uint64_t threads_count,
                       uint64_t tasks_capacity)
:   logger_(std::move(logger)),
    threads_max_count_(threads_count),
    tasks_max_capacity_(tasks_capacity)
{
    std::string name_(name);
    threads_.reserve(threads_max_count_);
    for (uint64_t serial = 0; serial < threads_max_count_; ++serial) {
        std::string thread_name(name_ + std::string("#") + std::to_string(serial));

        threads_.emplace_back(&ThreadPool::run, this, thread_name);
        ThreadHelpers::set_name(threads_.back().native_handle(), thread_name);
    }
}

void ThreadPool::wait_all()
{
    std::unique_lock<std::mutex> lock(task_results_all_mutex_);
    task_results_all_condition_.wait(lock, [this]()->bool {
        return ((statistic_.tasks_completed_count + statistic_.tasks_refused_count) == statistic_.tasks_last_id);
    });
}

// --------------------------------------------------------

void ThreadPool::run(const std::string thread_name)
{
    ThreadHelpers::block_signals();
    while (!threads_stop_) {
        std::unique_lock<std::mutex> lock(tasks_mutex_);
        tasks_condition_.wait(lock, [this]()->bool { return !tasks_.empty() || threads_stop_; });

        if (!tasks_.empty() && !threads_stop_) {
            auto task = std::move(tasks_.front());
            tasks_.pop();
            lock.unlock();

            LOG_TRACE(std::format("thread {}, start processing task #{}",
                thread_name, task.second.id_));
            try {
                task.first();
            }
            catch (std::exception& ex) {
                LOG_ERROR(std::format("thread {} while processing task #{}, exception: {}",
                    thread_name, task.second.id_, ex.what()));
            }
            LOG_TRACE(std::format("thread {}, end processing task #{}",
                thread_name, task.second.id_));

            if (task.second.cb_) task.second.cb_(task.first.get_result());
            ++statistic_.tasks_completed_count;
        }

        task_results_all_condition_.notify_all();
    }
}

} // namespace ThreadHelpers

} // namespace SocialNetwork
