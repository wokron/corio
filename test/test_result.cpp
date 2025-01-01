#include <corio/result.hpp>
#include <doctest/doctest.h>
#include <memory>
#include <stdexcept>

TEST_CASE("test result") {
    SUBCASE("create from result") {
        auto result = corio::Result<int>::from_result(42);
        CHECK(result.exception() == nullptr);
        CHECK(result.result() == 42);
    }

    SUBCASE("create from exception") {
        auto exception = std::make_exception_ptr(std::runtime_error("error"));
        auto result = corio::Result<int>::from_exception(exception);
        CHECK(result.exception() == exception);
        CHECK_THROWS_AS(result.result(), std::runtime_error);
    }

    SUBCASE("create from void result") {
        auto result = corio::Result<void>::from_result();
        CHECK(result.exception() == nullptr);
        CHECK_NOTHROW(result.result());
    }

    SUBCASE("create from exception for void type") {
        auto exception = std::make_exception_ptr(std::runtime_error("error"));
        auto result = corio::Result<void>::from_exception(exception);
        CHECK(result.exception() == exception);
        CHECK_THROWS_AS(result.result(), std::runtime_error);
    }

    SUBCASE("move-only type result") {
        auto result = corio::Result<std::unique_ptr<int>>::from_result(
            std::make_unique<int>(42));
        CHECK(result.exception() == nullptr);
        CHECK(result.result() != nullptr);
        CHECK(*result.result() == 42);
        CHECK(result.result() != nullptr); // Not moved
    }

    SUBCASE("exception as result") {
        auto exception = std::make_exception_ptr(std::runtime_error("error"));
        auto result = corio::Result<std::exception_ptr>::from_result(exception);
        CHECK(result.exception() == nullptr);
        CHECK(result.result() == exception);
    }
}