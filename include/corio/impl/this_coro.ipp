#pragma once

#include "asio.hpp"
#include "corio/lazy.hpp"
#include "corio/this_coro.hpp"
#include <optional>

namespace corio::this_coro {

namespace detail {

struct YieldAwaiter {
    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        CORIO_ASSERT(promise.executor(), "The executor is not set");
        auto &strand = promise.strand();
        asio::post(strand, [h = handle, c = cancelled] {
            bool is_cancelled = *c;
            if (!is_cancelled) {
                h.resume();
            }
        });
    }

    void await_resume() noexcept {}

    ~YieldAwaiter() { *cancelled = true; }

    std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
};

template <typename Rep, typename Period> struct SleepAwaiter {
    explicit SleepAwaiter(std::chrono::duration<Rep, Period> duration)
        : duration(duration) {}

    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        CORIO_ASSERT(promise.executor(), "The executor is not set");
        auto &strand = promise.strand();
        timer = asio::steady_timer{strand, duration};
        timer.value().async_wait([h = handle](const asio::error_code &error) {
            if (!error) {
                h.resume();
            }
        });
    }

    void await_resume() noexcept {}

    ~SleepAwaiter() { timer.value().cancel(); }

    std::chrono::duration<Rep, Period> duration;
    std::optional<asio::steady_timer> timer;
};

} // namespace detail

inline corio::Lazy<void> yield() { co_await detail::YieldAwaiter{}; }

template <typename Rep, typename Period>
inline corio::Lazy<void> sleep(std::chrono::duration<Rep, Period> duration) {
    co_await detail::SleepAwaiter<Rep, Period>(duration);
}

template <typename Rep, typename Period, typename Return>
inline corio::Lazy<Return> sleep(std::chrono::duration<Rep, Period> duration,
                                 Return return_value) {
    co_await detail::SleepAwaiter<Rep, Period>(duration);
    co_return return_value;
}

} // namespace corio::this_coro