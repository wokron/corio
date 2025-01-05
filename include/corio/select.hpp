#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/select.hpp"
#include "corio/gather.hpp"
#include <tuple>

namespace corio {

template <detail::awaitable... Awaitables>
inline auto select(Awaitables &&...awaitables) {
    auto awaitables_tuple = std::make_tuple(
        detail::keep_ref(std::forward<Awaitables>(awaitables))...);
    using Tuple = decltype(awaitables_tuple);

    return detail::TupleSelectAwaiter<Tuple>(std::move(awaitables_tuple));
}

template <detail::awaitable_iterable Iterable>
inline auto select(Iterable &&iterable) {
    return detail::IterSelectAwaiter<Iterable>(
        std::forward<Iterable>(iterable));
}

} // namespace corio