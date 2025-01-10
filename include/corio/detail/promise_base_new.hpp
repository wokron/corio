#pragma once

#include <coroutine>

namespace corio::detail {

class Background;

class PromiseBase {
public:
    void set_background(const Background *background) {
        background_ = background;
    }

    const Background *background() const { return background_; }

private:
    const Background *background_ = nullptr;
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