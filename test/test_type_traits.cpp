#include <corio/detail/this_coro.hpp>
#include <corio/detail/type_traits.hpp>
#include <corio/lazy.hpp>
#include <doctest/doctest.h>
#include <type_traits>
#include <variant>

using namespace corio::detail;

TEST_CASE("test type traits") {
    SUBCASE("test void_to_monostate_t") {
        static_assert(
            std::is_same_v<void_to_monostate_t<void>, std::monostate>);
        static_assert(std::is_same_v<void_to_monostate_t<int>, int>);
    }

    SUBCASE("test unique_types_t") {
        static_assert(std::is_same_v<unique_types_t<std::variant<int, int>>,
                                     std::variant<int>>);
        static_assert(
            std::is_same_v<unique_types_t<std::variant<int, int, double>>,
                           std::variant<int, double>>);
    }

    SUBCASE("test awaitable_return_t") {
        static_assert(
            std::is_same_v<awaitable_return_t<corio::Lazy<int>>, int>);
        static_assert(
            std::is_same_v<awaitable_return_t<corio::Lazy<void>>, void>);
    }

    SUBCASE("test at_position_t") {
        static_assert(std::is_same_v<at_position_t<0, int, double>, int>);
        static_assert(std::is_same_v<at_position_t<1, int, double>, double>);
    }

    SUBCASE("test safe_variant_t") {
        static_assert(std::is_same_v<safe_variant_t<int, double>,
                                     std::variant<int, double>>);
        static_assert(std::is_same_v<safe_variant_t<void, double>,
                                     std::variant<std::monostate, double>>);
    }

    SUBCASE("test safe_simplified_variant_t") {
        static_assert(std::is_same_v<safe_simplified_variant_t<int>, int>);
        static_assert(std::is_same_v<safe_simplified_variant_t<int, double>,
                                     std::variant<int, double>>);
        static_assert(std::is_same_v<safe_simplified_variant_t<void, double>,
                                     std::variant<std::monostate, double>>);
    }

    SUBCASE("test apply_each_t") {
        static_assert(
            std::is_same_v<apply_each_t<std::variant, std::tuple<int, double>>,
                           std::variant<int, double>>);
        static_assert(
            std::is_same_v<apply_each_t<std::variant, std::tuple<int>>,
                           std::variant<int>>);
    }

    SUBCASE("test unique_safe_simplified_variant_t") {
        static_assert(
            std::is_same_v<unique_safe_simplified_variant_t<int>, int>);
        static_assert(
            std::is_same_v<unique_safe_simplified_variant_t<int, double>,
                           std::variant<int, double>>);
        static_assert(
            std::is_same_v<unique_safe_simplified_variant_t<void, double>,
                           std::variant<std::monostate, double>>);
        static_assert(
            std::is_same_v<unique_safe_simplified_variant_t<void, void, int>,
                           std::variant<std::monostate, int>>);
        static_assert(
            std::is_same_v<unique_safe_simplified_variant_t<int, int>, int>);
    }
}