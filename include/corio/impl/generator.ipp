#pragma once

#include "corio/detail/assert.hpp"
#include "corio/generator.hpp"
#include <asio.hpp>
#include <functional>

namespace corio {

template <typename T>
Generator<T> &Generator<T>::operator=(Generator &&other) noexcept {
    if (this != &other) {
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

template <typename T> Generator<T>::~Generator() {
    if (handle_) {
        handle_.destroy();
    }
}

template <typename T>
template <typename PromiseType>
std::coroutine_handle<typename Generator<T>::promise_type>
Generator<T>::chain_coroutine(
    std::coroutine_handle<PromiseType> caller_handle) {
    CORIO_ASSERT(handle_, "The handle is null");
    promise_type &promise = handle_.promise();
    PromiseType &caller_promise = caller_handle.promise();
    promise.set_context(caller_promise.context());
    promise.set_caller_handle(caller_handle);
    return handle_;
}

namespace detail {

template <typename T> struct NextAwaiter {
public:
    NextAwaiter(Generator<T> &gen) : gen_(gen) {}

    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<PromiseType> caller_handle) {
        return gen_.chain_coroutine(caller_handle);
    }

    bool await_resume() const noexcept { return gen_.has_current(); }

private:
    Generator<T> &gen_;
};

} // namespace detail

template <typename T> auto Generator<T>::operator co_await() noexcept {
    return detail::NextAwaiter<T>(*this);
}

template <typename T> bool Generator<T>::has_current() const {
    CORIO_ASSERT(handle_ != nullptr, "Invalid generator");
    promise_type &promise = handle_.promise();
    return promise.has_value();
}

template <typename T> T Generator<T>::current() {
    CORIO_ASSERT(handle_ != nullptr, "Invalid generator");
    promise_type &promise = handle_.promise();
    CORIO_ASSERT(has_current(), "No more elements");
    return std::move(promise.value().result());
}

template <typename T, typename Fn>
Lazy<void> async_for_each(Generator<T> generator, Fn &&fn) {
    while (co_await generator) {
        fn(generator.current());
    }
}

} // namespace corio