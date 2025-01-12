#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/context.hpp"
#include "corio/detail/this_coro.hpp"
#include "corio/operation.hpp"
#include <asio.hpp>
#include <coroutine>

namespace corio {
template <typename T> class Task;

template <typename Executor, detail::awaitable Awaitable>
[[nodiscard]] Task<detail::awaitable_return_t<Awaitable>>
spawn(const Executor &executor, Awaitable aw);
}; // namespace corio

namespace corio::detail {

class PromiseBase {
public:
    ExecutorAwaiter await_transform(const executor_t &) {
        auto executor = context_->runner.get_executor();
        return {.executor = executor};
    }

    auto await_transform(const yield_t &) {
        return asio::post(context_->runner.get_executor(), corio::use_corio);
    }

    template <typename Rep, typename Period>
    auto await_transform(const std::chrono::duration<Rep, Period> &duration) {
        return SleepAwaiter(duration);
    }

    template <typename Clock, typename Duration>
    auto await_transform(
        const std::chrono::time_point<Clock, Duration> &time_point) {
        return SleepAwaiter(time_point);
    }

    template <typename Clock>
    auto await_transform(asio::basic_waitable_timer<Clock> &timer) {
        return timer.async_wait(corio::use_corio);
    }

    template <typename Executor>
    auto await_transform(const Executor &executor) {
        return ExecutorSwitchAwaiter(executor);
    }

    template <awaitable Awaitable>
    Awaitable &&await_transform(Awaitable &&awaitable) {
        return std::forward<Awaitable>(awaitable);
    }

    template <typename T> auto await_transform(std::future<T> &future) {
        return corio::spawn(asio::system_executor(), await_future(future));
    }

public:
    void set_context(TaskContext *context) { context_ = context; }

    TaskContext *context() const { return context_; }

private:
    TaskContext *context_ = nullptr;
};

struct FinalAwaiter {
    bool await_ready() noexcept { return ready; }

    template <typename PromiseType>
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<PromiseType> handle) noexcept;

    void await_resume() noexcept {}

    bool ready = false;
};

template <typename PromiseType>
inline std::coroutine_handle<> FinalAwaiter::await_suspend(
    std::coroutine_handle<PromiseType> handle) noexcept {
    auto &promise = handle.promise();
    auto caller_handle = promise.caller_handle();
    if (caller_handle) {
        return caller_handle;
    }
    return std::noop_coroutine();
}

} // namespace corio::detail