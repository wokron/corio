#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/background.hpp"
#include "corio/detail/lazy_promise.hpp"
#include "corio/lazy.hpp"

namespace corio {

template <typename T>
inline Lazy<T> &Lazy<T>::operator=(Lazy<T> &&other) noexcept {
    if (this != &other) {
        handle_ = std::exchange(other.handle_, nullptr);
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

template <typename T> inline Result<T> &Lazy<T>::get_result() {
    CORIO_ASSERT(handle_, "The handle is null");
    return handle_.promise().get_result();
}

template <typename T> inline const Result<T> &Lazy<T>::get_result() const {
    CORIO_ASSERT(handle_, "The handle is null");
    return handle_.promise().get_result();
}

template <typename T>
inline void Lazy<T>::set_background(detail::Background *background) {
    CORIO_ASSERT(handle_, "The handle is null");
    return handle_.promise().set_background(background);
}

template <typename T>
inline detail::Background *Lazy<T>::get_background() const {
    CORIO_ASSERT(handle_, "The handle is null");
    return handle_.promise().background();
}

template <typename T> inline void Lazy<T>::execute() {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    const detail::Background *bg = promise.background();
    CORIO_ASSERT(bg != nullptr, "The background is not set");
    asio::post(bg->runner.get_executor(), [h = handle_] { h.resume(); });
}

template <typename T>
template <typename PromiseType>
inline std::coroutine_handle<typename Lazy<T>::promise_type>
Lazy<T>::chain_coroutine(std::coroutine_handle<PromiseType> caller_handle) {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    PromiseType &caller_promise = caller_handle.promise();
    promise.set_background(caller_promise.background());
    promise.set_caller_handle(caller_handle);
    return handle_;
}

namespace detail {

template <typename T> class LazyAwaiter {
public:
    explicit LazyAwaiter(Lazy<T> &lazy) : lazy_(lazy) {}

    bool await_ready() { return lazy_.is_finished(); }

    template <typename PromiseType>
    std::coroutine_handle<typename Lazy<T>::promise_type>
    await_suspend(std::coroutine_handle<PromiseType> caller_handle) {
        return lazy_.chain_coroutine(caller_handle);
    }

    T await_resume() {
        Result<T> result = std::move(lazy_.get_result());
        lazy_.reset(); // Destroy the handle
        if constexpr (std::is_void_v<T>) {
            result.result();
            return;
        } else {
            return std::move(result.result());
        }
    }

private:
    Lazy<T> &lazy_;
};

} // namespace detail

template <typename T> inline auto Lazy<T>::operator co_await() {
    return detail::LazyAwaiter<T>(*this);
}

} // namespace corio