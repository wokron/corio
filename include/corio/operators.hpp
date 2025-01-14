#pragma once

#include "corio/detail/collective.hpp"
#include "corio/detail/operators.hpp"

namespace corio::awaitable_operators {

template <detail::awaitable Awaitable1, detail::awaitable Awaitable2>
inline auto operator&&(Awaitable1 &&awaitable1, Awaitable2 &&awaitable2) {
    return detail::TryGatherGenerator<Awaitable1, Awaitable2>(
        detail::keep_ref(std::forward<Awaitable1>(awaitable1)),
        detail::keep_ref(std::forward<Awaitable2>(awaitable2)));
}

template <typename... Ts, detail::awaitable Awaitable>
inline auto operator&&(detail::TryGatherGenerator<Ts...> &&generator,
                       Awaitable &&awaitable) {
    return generator.append(
        detail::keep_ref(std::forward<Awaitable>(awaitable)));
}

template <detail::awaitable Awaitable1, detail::awaitable Awaitable2>
inline auto operator||(Awaitable1 &&awaitable1, Awaitable2 &&awaitable2) {
    return detail::SelectGenerator<Awaitable1, Awaitable2>(
        detail::keep_ref(std::forward<Awaitable1>(awaitable1)),
        detail::keep_ref(std::forward<Awaitable2>(awaitable2)));
}

template <typename... Ts, detail::awaitable Awaitable>
inline auto operator||(detail::SelectGenerator<Ts...> &&generator,
                       Awaitable &&awaitable) {
    return generator.append(
        detail::keep_ref(std::forward<Awaitable>(awaitable)));
}

template <detail::awaitable Awaitable1, detail::awaitable Awaitable2>
inline auto operator&(Awaitable1 &&awaitable1, Awaitable2 &&awaitable2) {
    return detail::GatherGenerator<Awaitable1, Awaitable2>(
        detail::keep_ref(std::forward<Awaitable1>(awaitable1)),
        detail::keep_ref(std::forward<Awaitable2>(awaitable2)));
}

template <typename... Ts, detail::awaitable Awaitable>
inline auto operator&(detail::GatherGenerator<Ts...> &&generator,
                      Awaitable &&awaitable) {
    return generator.append(
        detail::keep_ref(std::forward<Awaitable>(awaitable)));
}

} // namespace corio::awaitable_operators