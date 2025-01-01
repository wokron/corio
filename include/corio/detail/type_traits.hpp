#ifndef CORIO_DETAIL_TYPE_TRAITS_HPP
#define CORIO_DETAIL_TYPE_TRAITS_HPP

#include <type_traits>
#include <variant>

namespace corio::detail {

template <typename T>
using void_to_monostate_t =
    std::conditional_t<std::is_void_v<T>, std::monostate, T>;

} // namespace corio

#endif // CORIO_DETAIL_TYPE_TRAITS_HPP