#pragma once

#include <coroutine>
#include <iterator>

namespace corio::detail {

template <typename Awaiter, typename Promise = void>
concept awaiter =
    requires(Awaiter awaiter, std::coroutine_handle<Promise> handle) {
        { awaiter.await_ready() };
        { awaiter.await_suspend(handle) };
        { awaiter.await_resume() };
    };

template <typename Awaitable, typename Promise = void>
concept awaitable =
    awaiter<Awaitable, Promise> || requires(Awaitable awaiter) {
                                       { awaiter.operator co_await() };
                                   };

template <typename Iterable>
concept iterable = requires(Iterable iterable) {
                       { std::begin(iterable) };
                       { std::end(iterable) };
                   };

template <typename Iterable>
concept awaitable_iterable =
    iterable<Iterable> && awaitable<std::iter_value_t<Iterable>>;

} // namespace corio::detail