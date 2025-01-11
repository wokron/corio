#pragma once

#include "corio/result.hpp"
#include <coroutine>
#include <utility>

namespace corio {

namespace detail {
template <typename T> class LazyPromise;
class TaskContext;
} // namespace detail

template <typename T> class [[nodiscard]] Lazy {
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

public:
    std::coroutine_handle<promise_type> release();

    void reset(std::coroutine_handle<promise_type> handle = nullptr);

    std::coroutine_handle<promise_type> get() const { return handle_; }

public:
    bool is_finished() const;

    Result<T> &get_result();

    const Result<T> &get_result() const;

    void set_context(detail::TaskContext *context);

    detail::TaskContext *get_context() const;

    void execute();

    template <typename PromiseType>
    std::coroutine_handle<promise_type>
    chain_coroutine(std::coroutine_handle<PromiseType> caller_handle);

    auto operator co_await();

private:
    std::coroutine_handle<promise_type> handle_;
};

} // namespace corio

#include "corio/impl/lazy.ipp"