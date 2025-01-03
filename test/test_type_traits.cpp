#include <corio/detail/this_coro.hpp>
#include <corio/detail/type_traits.hpp>
#include <corio/lazy.hpp>
#include <doctest/doctest.h>

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

    SUBCASE("test awaitable_return_type") {
        static_assert(
            std::is_same_v<awaitable_return_t<corio::Lazy<int>>, int>);
        static_assert(
            std::is_same_v<awaitable_return_t<corio::Lazy<void>>, void>);
        static_assert(
            std::is_same_v<
                awaitable_return_t<corio::this_coro::detail::YieldAwaiter>,
                void>);
    }
}