#pragma once

#include "corio/detail/background.hpp"
#include "corio/detail/serial_runner.hpp"
#include <asio.hpp>
#include <coroutine>
#include <mutex>
#include <optional>

namespace corio::detail {

struct executor_t {
    constexpr executor_t() noexcept {}
};

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

template <typename Executor> class ExecutorSwitchAwaiter {
public:
    explicit ExecutorSwitchAwaiter(const Executor &executor)
        : executor_(executor) {}

public:
    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        Background *bg = promise.background();
        auto &old_runner = bg->runner;
        if (old_runner.get_executor() == executor_) {
            return false;
        }
        auto new_runner = SerialRunner(executor_);

        {
            std::lock_guard<std::mutex> lock(*(bg->mu_ptr));
            *(bg->curr_runner_ptr) = new_runner;
            bg->runner = new_runner;
            asio::post(new_runner.get_executor(),
                       [h = handle]() { h.resume(); });
        }
        return true;
    }

    void await_resume() noexcept {}

private:
    Executor executor_;
};

} // namespace corio::detail