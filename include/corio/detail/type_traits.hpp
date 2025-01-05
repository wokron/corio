#pragma once

#include "corio/detail/concepts.hpp"
#include <cstddef>
#include <type_traits>
#include <variant>

namespace corio::detail {

template <typename T>
using void_to_monostate =
    std::conditional<std::is_void_v<T>, std::monostate, T>;

template <typename T>
using void_to_monostate_t = typename void_to_monostate<T>::type;

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

template <template <typename...> typename Class, typename Tuple>
struct apply_each;

template <template <typename...> typename Class, typename... Ts>
struct apply_each<Class, std::tuple<Ts...>> {
    using type = Class<Ts...>;
};

template <template <typename...> typename Class, typename Tuple>
using apply_each_t = typename apply_each<Class, Tuple>::type;

template <std::size_t I, typename... Ts>
using at_position_t = std::tuple_element_t<I, std::tuple<Ts...>>;

template <typename... Ts>
using safe_variant_t = std::variant<void_to_monostate_t<Ts>...>;

template <typename... Ts>
using safe_simplified_variant_t =
    std::conditional_t<sizeof...(Ts) == 1, at_position_t<0, Ts...>,
                       safe_variant_t<Ts...>>;

template <typename... Ts>
using unique_safe_simplified_variant_t =
    apply_each_t<safe_simplified_variant_t, unique_types_t<std::tuple<Ts...>>>;

template <typename Tuple, template <typename> typename Trait>
struct transform_tuple;

template <template <typename> typename Trait, typename... Ts>
struct transform_tuple<std::tuple<Ts...>, Trait> {
    using type = std::tuple<typename Trait<Ts>::type...>;
};

template <typename Tuple, template <typename> typename Trait>
using transform_tuple_t = typename transform_tuple<Tuple, Trait>::type;

template <typename Ref> struct unwrap_reference {
    using type = Ref;
};

template <typename T> struct unwrap_reference<std::reference_wrapper<T>> {
    using type = T;
};

template <typename Ref>
using unwrap_reference_t = typename unwrap_reference<Ref>::type;

} // namespace corio::detail