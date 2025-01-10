#pragma once

#include "asio/steady_timer.hpp"
#include "corio/detail/this_coro.hpp"
#include "corio/lazy.hpp"
#include "corio/operation.hpp"
#include "corio/this_coro.hpp"
#include <asio.hpp>
#include <optional>

namespace corio::this_coro {

inline auto yield() { return detail::YieldAwaiter(); }

template <typename Rep, typename Period>
inline auto sleep_for(const std::chrono::duration<Rep, Period> &duration) {
    return detail::SleepAwaiter(duration);
}

template <typename Clock, typename Duration>
inline auto
sleep_until(const std::chrono::time_point<Clock, Duration> &time_point) {
    return detail::SleepAwaiter(time_point);
}

inline auto run_on(const asio::any_io_executor &executor) {
    return detail::SwitchExecutorAwaiter(executor);
}

} // namespace corio::this_coro