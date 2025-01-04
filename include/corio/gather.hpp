#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/try_gather.hpp"

namespace corio {

template <detail::awaitable... Awaitables>
auto try_gather(Awaitables &&...awaitables) noexcept {
    return detail::TupleTryGatherAwaiter<Awaitables...>(
        std::forward<Awaitables>(awaitables)...);
}

template <detail::awaitable_iterable Iterable>
auto try_gather(Iterable &&iterable) noexcept {
    return detail::IterTryGatherAwaiter<Iterable>(
        std::forward<Iterable>(iterable));
}

} // namespace corio