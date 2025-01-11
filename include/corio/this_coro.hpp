#pragma once

#include "corio/detail/this_coro.hpp"

namespace corio::this_coro {

inline constexpr corio::detail::executor_t executor;

inline auto yield();

template <typename Rep, typename Period>
inline auto sleep_for(const std::chrono::duration<Rep, Period> &duration);

template <typename Clock, typename Duration>
inline auto
sleep_until(const std::chrono::time_point<Clock, Duration> &time_point);

template <typename Executor> inline auto roam_to(const Executor &executor);

} // namespace corio::this_coro

#include "corio/impl/this_coro.ipp"