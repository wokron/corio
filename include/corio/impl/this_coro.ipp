#pragma once

#include "asio/steady_timer.hpp"
#include "corio/detail/this_coro.hpp"
#include "corio/lazy.hpp"
#include "corio/operation.hpp"
#include "corio/this_coro.hpp"
#include <asio.hpp>
#include <optional>

namespace corio::this_coro {

inline corio::Lazy<void> yield() {
    co_await asio::post(co_await corio::this_coro::executor, corio::use_corio);
}

template <typename Rep, typename Period>
inline corio::Lazy<void>
sleep_for(const std::chrono::duration<Rep, Period> &duration) {
    asio::steady_timer timer(co_await corio::this_coro::executor, duration);
    co_await timer.async_wait(corio::use_corio);
}

template <typename Clock, typename Duration>
inline corio::Lazy<void>
sleep_until(const std::chrono::time_point<Clock, Duration> &time_point) {
    asio::steady_timer timer(co_await corio::this_coro::executor, time_point);
    co_await timer.async_wait(corio::use_corio);
}

inline corio::Lazy<void> run_on(const asio::any_io_executor &executor) {
    co_await asio::post(executor, corio::use_corio);
}

} // namespace corio::this_coro