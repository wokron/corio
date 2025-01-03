#pragma once

#include "asio.hpp"
#include "corio/detail/this_coro.hpp"
#include "corio/lazy.hpp"
#include "corio/this_coro.hpp"
#include <optional>

namespace corio::this_coro {

inline corio::Lazy<void> yield() { co_await detail::YieldAwaiter{}; }

template <typename Rep, typename Period>
inline corio::Lazy<void> sleep(std::chrono::duration<Rep, Period> duration) {
    co_await detail::SleepAwaiter<Rep, Period>(duration);
}

template <typename Rep, typename Period, typename Return>
inline corio::Lazy<Return> sleep(std::chrono::duration<Rep, Period> duration,
                                 Return return_value) {
    co_await detail::SleepAwaiter<Rep, Period>(duration);
    co_return return_value;
}

inline corio::Lazy<void> run_on(asio::any_io_executor executor) {
    co_await detail::SwitchExecutorAwaiter{executor};
}

} // namespace corio::this_coro