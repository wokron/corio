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

} // namespace detail

} // namespace corio::this_coro