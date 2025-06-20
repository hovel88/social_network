#pragma once

#include <any>
#include <functional>

namespace SocialNetwork {

namespace ThreadPooling {

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

} // namespace ThreadPooling

} // namespace SocialNetwork
