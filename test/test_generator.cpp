#include <asio.hpp>
#include <corio/detail/background.hpp>
#include <corio/generator.hpp>
#include <corio/lazy.hpp>
#include <doctest/doctest.h>
#include <stdexcept>

TEST_CASE("test generator") {

    SUBCASE("generator basic") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };

        auto g = [&](bool &called) -> corio::Lazy<void> {
            auto gen = f();
            std::vector<int> arr;

            while (co_await gen) {
                arr.push_back(gen.current());
            }

            CHECK(arr == std::vector<int>{1, 2, 3});
            called = true;
        };

        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        bool called = false;
        auto lazy = g(called);
        lazy.set_background(&bg);
        lazy.execute();

        io_context.run();

        CHECK(called);
    }

    SUBCASE("generator with exception") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            throw std::runtime_error("error");
            co_yield 3;
        };

        auto g = [&](bool &called) -> corio::Lazy<void> {
            bool catched = false;
            auto gen = f();
            std::vector<int> arr;
            try {
                while (co_await gen) {
                    arr.push_back(gen.current());
                }
            } catch (const std::runtime_error &e) {
                catched = true;
                CHECK(std::string(e.what()) == "error");
            }
            CHECK(catched);
            CHECK(arr == std::vector<int>{1, 2});
            called = true;
        };

        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        bool called = false;
        auto lazy = g(called);
        lazy.set_background(&bg);
        lazy.execute();

        io_context.run();

        CHECK(called);
    }

    SUBCASE("generator with move-only type") {
        auto f = []() -> corio::Generator<std::unique_ptr<int>> {
            co_yield std::make_unique<int>(1);
            co_yield std::make_unique<int>(2);
            co_yield std::make_unique<int>(3);
        };

        auto g = [&](bool &called) -> corio::Lazy<void> {
            auto gen = f();
            std::vector<std::unique_ptr<int>> arr;

            while (co_await gen) {
                arr.push_back(gen.current());
            }

            CHECK(arr.size() == 3);
            CHECK(*arr[0] == 1);
            CHECK(*arr[1] == 2);
            CHECK(*arr[2] == 3);

            called = true;
        };

        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        bool called = false;
        auto lazy = g(called);
        lazy.set_background(&bg);
        lazy.execute();

        io_context.run();

        CHECK(called);
    }

    SUBCASE("use async for-loop") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };

        auto g = [&](bool& called) -> corio::Lazy<void> {
            std::vector<int> arr;
            CORIO_ASYNC_FOR(auto e, f()) { arr.push_back(e); }
            CHECK(arr == std::vector<int>{1, 2, 3});
            called = true;
        };

        asio::io_context io_context;
        corio::detail::Background bg = {
            .runner = asio::make_strand<asio::any_io_executor>(
                io_context.get_executor()),
        };

        bool called = false;
        auto lazy = g(called);
        lazy.set_background(&bg);
        lazy.execute();

        io_context.run();

        CHECK(called);
    }
}