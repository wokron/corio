#pragma once

#include "corio/detail/assert.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <coroutine>
#include <optional>
#include <utility>

namespace corio {

namespace detail {
template <typename T> class LazyPromise;
}

template <typename T> class LazyAwaiter;

template <typename T> class Lazy {
public:
    using promise_type = detail::LazyPromise<T>;

    explicit Lazy(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

public:
    Lazy() : handle_(nullptr) {}

    Lazy(const Lazy &) = delete;

    Lazy &operator=(const Lazy &) = delete;

    Lazy(Lazy &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    Lazy &operator=(Lazy &&other) noexcept;

    ~Lazy();

    operator bool() const { return handle_ != nullptr; }

    std::coroutine_handle<promise_type> release();

    void reset(std::coroutine_handle<promise_type> handle = nullptr);

    std::coroutine_handle<promise_type> get() const { return handle_; }

public:
    bool is_finished() const;

    Result<T> &get_result() {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return handle_.promise().get_result();
    }

    const Result<T> &get_result() const {
        CORIO_ASSERT(is_finished(), "The result is not ready");
        return handle_.promise().get_result();
    }

    void set_executor(asio::any_io_executor executor) {
        CORIO_ASSERT(handle_, "The handle is null");
        handle_.promise().set_executor(executor);
    }

    void set_strand(asio::strand<asio::any_io_executor> strand) {
        CORIO_ASSERT(handle_, "The handle is null");
        handle_.promise().set_strand(strand);
    }

    void get_executor() const {
        CORIO_ASSERT(handle_, "The handle is null");
        return handle_.promise().executor();
    }

    auto get_strand() const {
        CORIO_ASSERT(handle_, "The handle is null");
        return handle_.promise().strand();
    }

    void execute();

    template <typename PromiseType>
    std::coroutine_handle<promise_type>
    chain_coroutine(std::coroutine_handle<PromiseType> caller_handle);

    LazyAwaiter<T> operator co_await();

private:
    std::coroutine_handle<promise_type> handle_;
};

} // namespace corio

#include "corio/impl/lazy.ipp"