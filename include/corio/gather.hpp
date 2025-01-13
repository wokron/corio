#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/gather.hpp"
#include "corio/detail/try_gather.hpp"

namespace corio {

template <detail::awaitable... Awaitables>
auto gather(Awaitables &&...awaitables) noexcept {
    auto awaitables_tuple = std::make_tuple(
        detail::keep_ref(std::forward<Awaitables>(awaitables))...);
    using Tuple = decltype(awaitables_tuple);

    return detail::TupleGatherAwaiter<Tuple>(std::move(awaitables_tuple));
}

template <detail::awaitable_iterable Iterable>
auto gather(Iterable &&iterable) noexcept {
    return detail::IterGatherAwaiter<Iterable>(
        std::forward<Iterable>(iterable));
}

template <detail::awaitable... Awaitables>
auto try_gather(Awaitables &&...awaitables) noexcept {
    auto awaitables_tuple = std::make_tuple(
        detail::keep_ref(std::forward<Awaitables>(awaitables))...);
    using Tuple = decltype(awaitables_tuple);

    return detail::TupleTryGatherAwaiter<Tuple>(std::move(awaitables_tuple));
}

template <detail::awaitable_iterable Iterable>
auto try_gather(Iterable &&iterable) noexcept {
    return detail::IterTryGatherAwaiter<Iterable>(
        std::forward<Iterable>(iterable));
}

} // namespace corio