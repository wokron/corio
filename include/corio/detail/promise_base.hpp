#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/context.hpp"
#include "corio/detail/this_coro.hpp"
#include <asio.hpp>
#include <coroutine>

namespace corio::detail {

class PromiseBase {
public:
    ExecutorAwaiter await_transform(const executor_t &);

    auto await_transform(const yield_t &);

    template <typename Rep, typename Period>
    auto await_transform(const std::chrono::duration<Rep, Period> &duration);

    template <typename Clock, typename Duration>
    auto
    await_transform(const std::chrono::time_point<Clock, Duration> &time_point);

    template <typename Clock>
    auto await_transform(asio::basic_waitable_timer<Clock> &timer);

    template <typename Executor> auto await_transform(const Executor &executor);

    template <typename T> auto await_transform(std::future<T> &future);

    template <awaitable Awaitable>
    Awaitable &&await_transform(Awaitable &&awaitable) {
        return std::forward<Awaitable>(awaitable);
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

} // namespace corio::detail

#include "corio/detail/impl/promise_base.ipp"