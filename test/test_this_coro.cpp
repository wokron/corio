#include "asio/any_io_executor.hpp"
#include "asio/io_context.hpp"
#include <corio/lazy.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>

TEST_CASE("test this_coro") {
    auto f = [&]() -> corio::Lazy<asio::any_io_executor> {
        co_return co_await corio::this_coro::executor;
    };

    auto g = []() -> corio::Lazy<asio::strand<asio::any_io_executor>> {
        co_return co_await corio::this_coro::strand;
    };

    auto k = [&]() -> corio::Lazy<void> {
        auto r1 = co_await f();
        auto r2 = co_await corio::this_coro::executor;
        CHECK(r1 == r2);
        auto r3 = co_await g();
        auto r4 = co_await corio::this_coro::strand;
        CHECK(r3 == r4);
    };

    asio::io_context io1, io2;

    auto lazy1 = f();
    lazy1.set_executor(io1.get_executor());
    lazy1.execute();
    io1.run();
    auto ex1 = lazy1.get_result().result();

    auto lazy2 = f();
    lazy2.set_executor(io2.get_executor());
    lazy2.execute();
    io2.run();
    auto ex2 = lazy2.get_result().result();

    CHECK(ex1 != ex2);

    asio::io_context io3;

    auto lazy3 = k();
    lazy3.set_executor(io3.get_executor());
    lazy3.execute();
    io3.run();
    CHECK_NOTHROW(lazy3.get_result().result());
}