#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/gather.hpp"
#include "corio/detail/select.hpp"
#include "corio/detail/try_gather.hpp"
#include <tuple>

namespace corio::detail {

template <template <typename...> typename CollectAwaiter, typename... Ts>
class CollectAwaiterGenerator {
public:
    explicit CollectAwaiterGenerator(std::tuple<Ts...> awaitables)
        : awaitables_{std::move(awaitables)} {}

    explicit CollectAwaiterGenerator(Ts &&...ts)
        : awaitables_{std::forward<Ts>(ts)...} {}

    template <typename T>
    CollectAwaiterGenerator<CollectAwaiter, Ts..., T> append(T &&t) {
        return CollectAwaiterGenerator<CollectAwaiter, Ts..., T>(std::tuple_cat(
            std::move(awaitables_), std::make_tuple(std::forward<T>(t))));
    }

    auto operator co_await() {
        return CollectAwaiter<decltype(awaitables_)>{std::move(awaitables_)};
    }

private:
    std::tuple<Ts...> awaitables_;
};

template <typename... Ts>
using TryGatherGenerator =
    CollectAwaiterGenerator<TupleTryGatherAwaiter, Ts...>;

template <typename... Ts>
using SelectGenerator = CollectAwaiterGenerator<TupleSelectAwaiter, Ts...>;

template <typename... Ts>
using GatherGenerator = CollectAwaiterGenerator<TupleGatherAwaiter, Ts...>;

} // namespace corio::detail