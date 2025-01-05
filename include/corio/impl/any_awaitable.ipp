#pragma once

#include "corio/any_awaitable.hpp"
#include <utility>

namespace corio {

template <detail::awaitable... Awaitables>
Lazy<typename AnyAwaitable<Awaitables...>::ReturnType>
AnyAwaitable<Awaitables...>::do_co_await_() {
    return std::visit(
        [](auto &&arg) -> Lazy<ReturnType> {
            using Arg = std::decay_t<decltype(arg)>;
            static_assert(detail::awaitable<Arg>, "Arg is not awaitable");
            if constexpr (std::is_void_v<ReturnType>) {
                co_await arg;
                co_return;
            } else if constexpr (std::is_void_v<
                                     detail::awaitable_return_t<Arg>>) {
                co_await arg;
                co_return std::monostate{};
            } else {
                co_return co_await arg;
            }
        },
        *awaitable_);
}

template <detail::awaitable... Awaitables>
auto AnyAwaitable<Awaitables...>::operator co_await() {
    lazy_ = do_co_await_(); // Wrap everything in a Lazy
    return lazy_.value().operator co_await();
}

template <detail::awaitable... Awaitables>
std::variant<Awaitables...> AnyAwaitable<Awaitables...>::unwrap() {
    lazy_ = std::nullopt;
    return std::exchange(awaitable_, std::nullopt).value();
}

} // namespace corio