#pragma once

#include "corio/detail/background.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/this_coro.hpp"
#include <coroutine>

namespace corio::detail {

class PromiseBase {
public:
    ExecutorAwaiter await_transform(const executor_t &) {
        auto executor = background_->runner.get_executor();
        return {.executor = executor};
    }

    template <awaitable Awaitable>
    Awaitable &&await_transform(Awaitable &&awaitable) {
        return std::forward<Awaitable>(awaitable);
    }

public:
    void set_background(Background *background) {
        background_ = background;
    }

    Background *background() const { return background_; }

private:
    Background *background_ = nullptr;
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