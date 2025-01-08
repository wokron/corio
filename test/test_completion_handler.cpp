#include <asio.hpp>
#include <corio/detail/completion_handler.hpp>
#include <doctest/doctest.h>
#include <memory>
#include <type_traits>

TEST_CASE("test build result") {

    SUBCASE("build result with no arguments") {
        auto result = corio::detail::build_result();
        static_assert(std::is_same_v<decltype(result), corio::Result<void>>);
        CHECK_NOTHROW(result.result());
    }

    SUBCASE("build result with asio::error_code") {
        asio::error_code ec = asio::error::operation_aborted;
        auto result = corio::detail::build_result(ec);
        static_assert(std::is_same_v<decltype(result), corio::Result<void>>);
        CHECK_THROWS(result.result());
    }

    SUBCASE("build result with non-error single argument") {
        int arg = 42;
        auto result = corio::detail::build_result(arg);
        static_assert(std::is_same_v<decltype(result), corio::Result<int>>);
        CHECK(result.result() == 42);
    }

    SUBCASE("build result with error and single argument") {
        asio::error_code ec = asio::error::operation_aborted;
        int arg = -1;
        auto result = corio::detail::build_result(ec, arg);
        static_assert(std::is_same_v<decltype(result), corio::Result<int>>);
        CHECK_THROWS(result.result());
    }

    SUBCASE("build result with non-error multiple arguments") {
        int arg1 = 42;
        double arg2 = 3.14;
        auto result = corio::detail::build_result(arg1, arg2);
        static_assert(std::is_same_v<decltype(result),
                                     corio::Result<std::tuple<int, double>>>);
        auto [r1, r2] = result.result();
        CHECK(r1 == 42);
        CHECK(r2 == 3.14);
    }

    SUBCASE("build result with error and multiple arguments") {
        asio::error_code ec = asio::error::operation_aborted;
        int arg1 = -42;
        double arg2 = -3.14;
        auto result = corio::detail::build_result(ec, arg1, arg2);
        static_assert(std::is_same_v<decltype(result),
                                     corio::Result<std::tuple<int, double>>>);
        CHECK_THROWS(result.result());
    }
}