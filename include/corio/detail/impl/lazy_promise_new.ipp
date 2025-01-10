#pragma once

#include "corio/detail/lazy_promise_new.hpp"
#include "corio/lazy_new.hpp"

namespace corio::detail {

template <typename T>
inline corio::Lazy<T> LazyPromise<T>::get_return_object() {
    return corio::Lazy<T>(
        std::coroutine_handle<LazyPromise<T>>::from_promise(*this));
}

inline corio::Lazy<void> LazyPromise<void>::get_return_object() {
    return corio::Lazy<void>(
        std::coroutine_handle<LazyPromise<void>>::from_promise(*this));
}

} // namespace corio::detail
