#pragma once

#include "corio/detail/this_coro.hpp"

namespace corio::this_coro {

inline auto do_yield() { return corio::detail::YieldAwaiter{}; }

template <typename Rep, typename Period>
auto sleep_for(const std::chrono::duration<Rep, Period> &duration) {
    return corio::detail::SleepAwaiter(duration);
}

template <typename Clock, typename Duration>
auto sleep_until(const std::chrono::time_point<Clock, Duration> &time_point) {
    return corio::detail::SleepAwaiter(time_point);
}

template <typename Executor> inline auto roam_to(const Executor &executor) {
    return corio::detail::ExecutorSwitchAwaiter(executor);
}

} // namespace corio::this_coro