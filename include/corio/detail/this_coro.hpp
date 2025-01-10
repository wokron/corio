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

struct SwitchExecutorAwaiter {
    explicit SwitchExecutorAwaiter(const asio::any_io_executor &executor)
        : executor(executor) {}

    SwitchExecutorAwaiter(const SwitchExecutorAwaiter &) = delete;
    SwitchExecutorAwaiter &operator=(const SwitchExecutorAwaiter &) = delete;
    SwitchExecutorAwaiter(SwitchExecutorAwaiter &&) = default;
    SwitchExecutorAwaiter &operator=(SwitchExecutorAwaiter &&) = default;

    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        auto &promise = handle.promise();
        auto old_strand = promise.strand();
        if (old_strand.get_inner_executor() == executor) {
            return false;
        }
        promise.set_executor(executor);
        asio::post(handle.promise().strand(), [handle] { handle.resume(); });
        return true;
    }

    void await_resume() noexcept { finish_switch = true; }

    ~SwitchExecutorAwaiter() {
        if (!finish_switch) {
            // The coroutine is canceled before the executor is switched,
            // which is undefined behavior
            std::terminate();
        }
    }

    asio::any_io_executor executor;
    bool finish_switch = false;
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
        auto &strand = promise.strand();
        asio::post(strand, [h = handle, c = cancelled]() {
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
        auto &strand = promise.strand();
        timer = asio::steady_timer(strand, expire_time);
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

} // namespace detail

} // namespace corio::this_coro