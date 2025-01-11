#pragma once

#include "corio/detail/this_coro.hpp"

namespace corio::this_coro {

inline auto yield() { return corio::detail::YieldAwaiter{}; }

template <typename Rep, typename Period>
auto sleep_for(const std::chrono::duration<Rep, Period> &duration) {
    return corio::detail::SleepAwaiter(duration);
}

template <typename Clock, typename Duration>
auto sleep_until(const std::chrono::time_point<Clock, Duration> &time_point) {
    return corio::detail::SleepAwaiter(time_point);
}

} // namespace corio::this_coro