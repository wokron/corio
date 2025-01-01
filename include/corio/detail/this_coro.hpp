#pragma once

#include "asio/any_io_executor.hpp"
#include <asio.hpp>
#include <coroutine>

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

} // namespace detail

} // namespace corio::this_coro