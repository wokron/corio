#pragma once

#include "corio/detail/lazy_promise.hpp"
#include "corio/lazy.hpp"

namespace corio {

template <typename T>
inline Lazy<T> &Lazy<T>::operator=(Lazy<T> &&other) noexcept {
    if (this != &other) {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

template <typename T> inline Lazy<T>::~Lazy() {
    if (handle_) {
        handle_.destroy();
    }
}

template <typename T>
inline std::coroutine_handle<typename Lazy<T>::promise_type>
Lazy<T>::release() {
    std::coroutine_handle<promise_type> handle = handle_;
    handle_ = nullptr;
    return handle;
}

template <typename T>
inline void Lazy<T>::reset(std::coroutine_handle<promise_type> handle) {
    if (handle_) {
        handle_.destroy();
    }
    handle_ = handle;
}

template <typename T> inline bool Lazy<T>::is_finished() const {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    bool finished = promise.is_finished();
    CORIO_ASSERT(finished == handle_.done(),
                 "The promise and handle are inconsistent");
    return finished;
}

template <typename T> inline void Lazy<T>::execute() {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    CORIO_ASSERT(promise.executor(), "The executor is not set");
    auto &strand = promise.strand();
    asio::post(strand, [h = handle_] { h.resume(); });
}

template <typename T>
template <typename PromiseType>
inline std::coroutine_handle<typename Lazy<T>::promise_type>
Lazy<T>::chain_coroutine(std::coroutine_handle<PromiseType> caller_handle) {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    PromiseType &caller_promise = caller_handle.promise();
    promise.set_strand(caller_promise.strand());
    promise.set_caller_handle(caller_handle);
    return handle_;
}

template <typename T> class LazyAwaiter {
public:
    explicit LazyAwaiter(Lazy<T> &lazy) : lazy_(lazy) {}

    bool await_ready() { return lazy_.is_finished(); }

    template <typename PromiseType>
    std::coroutine_handle<typename Lazy<T>::promise_type>
    await_suspend(std::coroutine_handle<PromiseType> caller_handle) {
        update_strand_ =
            [caller_handle](asio::strand<asio::any_io_executor> strand) {
                caller_handle.promise().set_strand(strand);
            };
        return lazy_.chain_coroutine(caller_handle);
    }

    T await_resume() {
        update_strand_(lazy_.get_strand());
        Result<T> result = std::move(lazy_.get_result());
        lazy_.reset(); // Destroy the handle
        if constexpr (std::is_void_v<T>) {
            result.result();
        } else {
            return std::move(result.result());
        }
    }

private:
    Lazy<T> &lazy_;
    std::function<void(asio::strand<asio::any_io_executor>)> update_strand_;
};

template <typename T> inline LazyAwaiter<T> Lazy<T>::operator co_await() {
    return LazyAwaiter<T>(*this);
}

} // namespace corio