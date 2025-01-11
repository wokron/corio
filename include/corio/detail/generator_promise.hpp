#pragma once

#include "corio/detail/promise_base.hpp"
#include "corio/result.hpp"
#include <coroutine>
#include <optional>

namespace corio {
template <typename T> class Generator;
} // namespace corio

namespace corio::detail {

template <typename T> class GeneratorPromise : public PromiseBase {
public:
    corio::Generator<T> get_return_object() {
        return corio::Generator<T>{
            std::coroutine_handle<GeneratorPromise>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() { return {}; }
    FinalAwaiter final_suspend() noexcept { return {}; }

    void return_void() { value_ = std::nullopt; }
    void unhandled_exception() {
        value_ = corio::Result<T>::from_exception(std::current_exception());
    }
    FinalAwaiter yield_value(T &&v) {
        value_ = corio::Result<T>::from_result(std::forward<T>(v));
        return {};
    }

public:
    std::coroutine_handle<> caller_handle() const { return caller_handle_; }

    void set_caller_handle(std::coroutine_handle<> caller_handle) {
        caller_handle_ = caller_handle;
    }

    bool has_value() const { return value_.has_value(); }

    corio::Result<T> &value() { return value_.value(); }

private:
    std::optional<corio::Result<T>> value_ = std::nullopt;
    std::coroutine_handle<> caller_handle_ = nullptr;
};

} // namespace corio::detail