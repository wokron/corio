#pragma once

#include "corio/detail/concepts.hpp"
#include <cstddef>
#include <type_traits>
#include <variant>

namespace corio::detail {

template <typename T>
using void_to_monostate_t =
    std::conditional_t<std::is_void_v<T>, std::monostate, T>;

template <typename T, typename... Ts> struct unique_types_impl {
    using type = T;
};

template <template <typename...> typename Class, typename... Ts, typename U,
          typename... Us>
struct unique_types_impl<Class<Ts...>, U, Us...>
    : std::conditional_t<(std::is_same_v<U, Ts> || ...),
                         unique_types_impl<Class<Ts...>, Us...>,
                         unique_types_impl<Class<Ts..., U>, Us...>> {};

template <typename Class> struct unique_types;

template <template <typename...> typename Class, typename... Ts>
struct unique_types<Class<Ts...>> : public unique_types_impl<Class<>, Ts...> {};

template <typename Class>
using unique_types_t = typename unique_types<Class>::type;

template <awaiter Awaiter>
using awaiter_return_t = decltype(std::declval<Awaiter>().await_resume());

template <awaitable Awaitable> struct awaitable_return {
    using type = awaiter_return_t<
        decltype(std::declval<Awaitable>().operator co_await())>;
};

template <awaiter Awaiter> struct awaitable_return<Awaiter> {
    using type = awaiter_return_t<Awaiter>;
};

template <awaitable Awaitable>
using awaitable_return_t = typename awaitable_return<Awaitable>::type;

template <typename... Ts>
using safe_variant_t = std::variant<void_to_monostate_t<Ts>...>;

template <typename... Ts>
using unique_safe_variant_t = unique_types_t<safe_variant_t<Ts...>>;

template <std::size_t I, typename... Ts>
using at_position_t = std::tuple_element_t<I, std::tuple<Ts...>>;

template <typename... Ts>
using simplified_unique_safe_variant_t =
    std::conditional_t<sizeof...(Ts) == 1,
                       void_to_monostate_t<at_position_t<0, Ts...>>,
                       unique_safe_variant_t<Ts...>>;

} // namespace corio::detail