#include <asio.hpp>
#include <chrono>
#include <corio/detail/background.hpp>
#include <corio/lazy_new.hpp>
#include <doctest/doctest.h>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("test lazy new") {

    SUBCASE("basic") {
        auto f = []() -> corio::Lazy<int> { co_return 42; };

        auto lazy = f();
        CHECK(!lazy.is_finished());
        CHECK_THROWS(lazy.execute());

        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        lazy.set_background(&bg);
        CHECK_NOTHROW(lazy.execute());
        CHECK(!lazy.is_finished());

        io_context.run();
        CHECK(lazy.is_finished());
        CHECK(lazy.get_result().result() == 42);
    }

    SUBCASE("resouce manage") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };

        auto tmp = f();
        CHECK(tmp);

        corio::Lazy<int> lazy;
        CHECK(!lazy);

        lazy = std::move(tmp);
        CHECK(lazy);
        CHECK(!tmp);

        auto h = lazy.release();
        CHECK(!lazy);
        CHECK(h != nullptr);

        tmp.reset(h);
        CHECK(tmp);

        tmp.reset();
        CHECK(!tmp);
    }

    SUBCASE("nested function") {
        auto f1 = []() -> corio::Lazy<int> { co_return 1; };
        auto f2 = [&]() -> corio::Lazy<int> {
            auto n = co_await f1();
            co_return n + co_await f1();
        };

        auto lazy = f2();
        CHECK(!lazy.is_finished());
        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        lazy.set_background(&bg);
        lazy.execute();

        io_context.run();

        CHECK(lazy.is_finished());
        CHECK(lazy.get_result().result() == 2);
    }
}