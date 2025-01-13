#pragma once

#include "corio/detail/promise_base.hpp"
#include "corio/operation.hpp"

namespace corio {
template <typename T> class Task;

template <typename Executor, detail::awaitable Awaitable>
Task<detail::awaitable_return_t<Awaitable>> spawn(const Executor &executor,
                                                  Awaitable aw);
}; // namespace corio

namespace corio::detail {

inline ExecutorAwaiter PromiseBase::await_transform(const executor_t &) {
    auto executor = context_->runner.get_executor();
    return {.executor = executor};
}

inline auto PromiseBase::await_transform(const yield_t &) {
    return asio::post(context_->runner.get_executor(), corio::use_corio);
}

template <typename Rep, typename Period>
inline auto PromiseBase::await_transform(
    const std::chrono::duration<Rep, Period> &duration) {
    return SleepAwaiter(duration);
}

template <typename Clock, typename Duration>
inline auto PromiseBase::await_transform(
    const std::chrono::time_point<Clock, Duration> &time_point) {
    return SleepAwaiter(time_point);
}

template <typename Clock>
inline auto
PromiseBase::await_transform(asio::basic_waitable_timer<Clock> &timer) {
    return timer.async_wait(corio::use_corio);
}

template <typename Executor>
inline auto PromiseBase::await_transform(const Executor &executor) {
    return ExecutorSwitchAwaiter(executor);
}

template <typename T>
inline auto PromiseBase::await_transform(std::future<T> &future) {
    return corio::spawn(asio::system_executor(), await_future(future));
}

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