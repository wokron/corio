#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/gather.hpp"

namespace corio {

template <detail::awaitable... Awaitables>
auto gather(Awaitables &&...awaitables) noexcept {
    return detail::TupleGatherAwaiter<Awaitables...>(
        std::forward<Awaitables>(awaitables)...);
}

template <detail::awaitable_iterable Iterable>
auto gather(Iterable &&iterable) noexcept {
    return detail::IterGatherAwaiter<Iterable>(
        std::forward<Iterable>(iterable));
}

} // namespace corio