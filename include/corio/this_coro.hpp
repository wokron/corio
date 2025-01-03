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
inline auto sleep(std::chrono::duration<Rep, Period> duration);

template <typename Rep, typename Period, typename Return>
inline corio::Lazy<Return> sleep(std::chrono::duration<Rep, Period> duration,
                                 Return return_value);

inline auto run_on(asio::any_io_executor executor);

} // namespace corio::this_coro

#include "corio/impl/this_coro.ipp"