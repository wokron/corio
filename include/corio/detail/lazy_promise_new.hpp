#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/promise_base_new.hpp"
#include "corio/result.hpp"
#include <coroutine>
#include <optional>

namespace corio {
template <typename T> class Lazy;
} // namespace corio

namespace corio::detail {

struct FinalAwaiter {
    bool await_ready() noexcept { return ready; }

    template <typename PromiseType>
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<PromiseType> handle) noexcept;

    void await_resume() noexcept {}

    bool ready = false;
};

class LazyPromiseBase : public PromiseBase {
public:
    std::suspend_always initial_suspend() { return {}; }
    FinalAwaiter final_suspend() noexcept { return {destroy_when_exit_}; }

public:
    void set_caller_handle(std::coroutine_handle<> caller_handle) {
        caller_handle_ = caller_handle;
    }

    std::coroutine_handle<> caller_handle() const { return caller_handle_; }

    void set_destroy_when_exit(bool destroy_when_exit) {
        destroy_when_exit_ = destroy_when_exit;
    }

private:
    std::coroutine_handle<> caller_handle_ = nullptr;
    bool destroy_when_exit_ = false;
};

template <typename T> class LazyPromise : public LazyPromiseBase {
public:
    corio::Lazy<T> get_return_object();

    void return_value(T &&value) {
        result_ = corio::Result<T>::from_result(std::forward<T>(value));
    }

    void unhandled_exception() {
        result_ = corio::Result<T>::from_exception(std::current_exception());
    }

public:
    bool is_finished() const { return result_.has_value(); }

    corio::Result<T> &get_result() {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return result_.value();
    }

    const corio::Result<T> &get_result() const {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return result_.value();
    }

private:
    std::optional<corio::Result<T>> result_;
};

template <> class LazyPromise<void> : public LazyPromiseBase {
public:
    corio::Lazy<void> get_return_object();

    void return_void() { result_ = corio::Result<void>::from_result(); }

    void unhandled_exception() {
        result_ = corio::Result<void>::from_exception(std::current_exception());
    }

public:
    bool is_finished() const { return result_.has_value(); }

    corio::Result<void> &get_result() {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return result_.value();
    }

    const corio::Result<void> &get_result() const {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return result_.value();
    }

private:
    std::optional<corio::Result<void>> result_;
};

} // namespace corio::detail

#include "corio/detail/impl/lazy_promise_new.ipp"
