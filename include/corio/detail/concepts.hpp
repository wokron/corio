#pragma once

#include <coroutine>
#include <unistd.h>

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

} // namespace corio::detail