#pragma once

#include "corio/detail/assert.hpp"
#include <asio.hpp>
#include <coroutine>
#include <exception>
#include <optional>

namespace corio::this_coro {

namespace detail {

struct ExecutorPlaceholder {
    ExecutorPlaceholder() = default;
    ExecutorPlaceholder(const ExecutorPlaceholder &) = delete;
    ExecutorPlaceholder &operator=(const ExecutorPlaceholder &) = delete;
    ExecutorPlaceholder(ExecutorPlaceholder &&) = delete;
    ExecutorPlaceholder &operator=(ExecutorPlaceholder &&) = delete;
};

struct StrandPlaceholder {
    StrandPlaceholder() = default;
    StrandPlaceholder(const StrandPlaceholder &) = delete;
    StrandPlaceholder &operator=(const StrandPlaceholder &) = delete;
    StrandPlaceholder(StrandPlaceholder &&) = delete;
    StrandPlaceholder &operator=(StrandPlaceholder &&) = delete;
};

struct ExecutorAwaiter {
    bool await_ready() const noexcept { return true; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {}

    auto await_resume() const noexcept { return executor; }

    asio::any_io_executor executor;
};

struct StrandAwaiter {
    bool await_ready() const noexcept { return true; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {}

    auto await_resume() const noexcept { return strand; }

    asio::strand<asio::any_io_executor> strand;
};

struct YieldAwaiter {
    YieldAwaiter() = default;
    YieldAwaiter(const YieldAwaiter &) = delete;
    YieldAwaiter &operator=(const YieldAwaiter &) = delete;
    YieldAwaiter(YieldAwaiter &&) = default;
    YieldAwaiter &operator=(YieldAwaiter &&) = default;

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

    ~YieldAwaiter() {
        if (cancelled != nullptr) {
            *cancelled = true;
        }
    }

    std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
};

template <typename Rep, typename Period> struct SleepAwaiter {
    explicit SleepAwaiter(std::chrono::duration<Rep, Period> duration)
        : duration(duration) {}
    SleepAwaiter(const SleepAwaiter &) = delete;
    SleepAwaiter &operator=(const SleepAwaiter &) = delete;
    SleepAwaiter(SleepAwaiter &&) = default;
    SleepAwaiter &operator=(SleepAwaiter &&) = default;

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

    ~SleepAwaiter() {
        if (timer.has_value()) {
            timer.value().cancel();
        }
    }

    std::chrono::duration<Rep, Period> duration;
    std::optional<asio::steady_timer> timer;
};

} // namespace detail

} // namespace corio::this_coro