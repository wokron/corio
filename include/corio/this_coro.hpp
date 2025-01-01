#pragma once

#include "corio/detail/this_coro.hpp"

namespace corio::this_coro {

inline detail::ExecutorPlaceholder executor;

inline detail::StrandPlaceholder strand;

} // namespace corio::this_coro