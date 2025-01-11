#pragma once

#include <asio.hpp>
#include <coroutine>
#include <optional>

namespace corio::detail {

struct executor_t {};

struct ExecutorAwaiter {
    bool await_ready() const noexcept { return true; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {}

    asio::any_io_executor await_resume() const noexcept { return executor; }

    asio::any_io_executor executor;
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
        auto executor = promise.background()->runner.get_executor();
        asio::post(executor, [h = handle, c = cancelled]() {
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

template <typename Time> struct SleepAwaiter {
    explicit SleepAwaiter(Time expire_time) : expire_time(expire_time) {}

    SleepAwaiter(const SleepAwaiter &) = delete;
    SleepAwaiter &operator=(const SleepAwaiter &) = delete;
    SleepAwaiter(SleepAwaiter &&) = default;
    SleepAwaiter &operator=(SleepAwaiter &&) = default;

    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        auto executor = promise.background()->runner.get_executor();
        timer = asio::steady_timer(executor, expire_time);
        timer.value().async_wait([h = handle](const asio::error_code &ec) {
            if (!ec) {
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

    Time expire_time;
    std::optional<asio::steady_timer> timer;
};

} // namespace corio::detail