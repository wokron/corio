#pragma once

#include "corio/detail/gather.hpp"

namespace corio {

template <detail::awaitable... Awaitables>
auto gather(Awaitables &&...awaitables) noexcept {
    return detail::TupleGatherAwaiter<Awaitables...>(
        std::forward<Awaitables>(awaitables)...);
}

} // namespace corio