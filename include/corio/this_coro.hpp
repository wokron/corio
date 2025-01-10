#pragma once

#include "corio/detail/this_coro.hpp"
#include "corio/detail/type_traits.hpp"
#include <chrono>
#include <optional>
#include <type_traits>

namespace corio {
template <typename T> class Lazy;
}

namespace corio::this_coro {

inline constexpr detail::ExecutorPlaceholder executor;

inline constexpr detail::StrandPlaceholder strand;

inline auto yield();

template <typename Rep, typename Period>
inline auto sleep_for(const std::chrono::duration<Rep, Period> &duration);

template <typename Clock, typename Duration>
inline auto
sleep_until(const std::chrono::time_point<Clock, Duration> &time_point);

inline auto run_on(const asio::any_io_executor &executor);

} // namespace corio::this_coro

#include "corio/impl/this_coro.ipp"