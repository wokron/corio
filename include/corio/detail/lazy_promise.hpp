#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/this_coro.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <coroutine>
#include <optional>
#include <type_traits>

namespace corio {
template <typename T> class Lazy;
}

namespace corio::detail {

struct FinalAwaiter {
    bool await_ready() noexcept { return ready; }

    template <typename PromiseType>
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<PromiseType> handle) noexcept;

    void await_resume() noexcept {}

    bool ready = false;
};

class LazyPromiseBase {
public:
    this_coro::detail::ExecutorAwaiter
    await_transform(const this_coro::detail::ExecutorPlaceholder &) {
        return {.executor = executor_};
    }

    this_coro::detail::StrandAwaiter
    await_transform(const this_coro::detail::StrandPlaceholder &) {
        CORIO_ASSERT(strand_.has_value(), "The strand is not set");
        return {.strand = strand_.value()};
    }

    template <detail::awaitable Awaitable>
    Awaitable await_transform(Awaitable &&awaitable) {
        return std::forward<Awaitable>(awaitable);
    }

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

    void set_executor(asio::any_io_executor executor) {
        executor_ = executor;
        strand_ = asio::make_strand(executor_);
    }

    const asio::any_io_executor &executor() const { return executor_; }

    void set_strand(asio::strand<asio::any_io_executor> strand) {
        strand_ = strand;
        executor_ = strand.get_inner_executor();
    }

    const asio::strand<asio::any_io_executor> &strand() const {
        CORIO_ASSERT(strand_.has_value(), "The strand is not set");
        return strand_.value();
    }

private:
    std::coroutine_handle<> caller_handle_ = nullptr;
    bool destroy_when_exit_ = false;
    asio::any_io_executor executor_;
    std::optional<asio::strand<asio::any_io_executor>> strand_;
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

#include "corio/detail/impl/lazy_promise.ipp"
