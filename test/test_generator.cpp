#include "asio/thread_pool.hpp"
#include "corio/gather.hpp"
#include "corio/runner.hpp"
#include <algorithm>
#include <corio/generator.hpp>
#include <doctest/doctest.h>
#include <stdexcept>

TEST_CASE("test generator") {

    SUBCASE("generator basic") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto gen = f();
            std::vector<int> arr;

            while (co_await gen) {
                arr.push_back(gen.current());
            }

            CHECK(arr == std::vector<int>{1, 2, 3});
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("generator with exception") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            throw std::runtime_error("error");
            co_yield 3;
        };

        auto g = [&]() -> corio::Lazy<void> {
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
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("generator with lazy") {
        auto f = [](int i) -> corio::Lazy<int> { co_return i; };

        auto g = [&]() -> corio::Generator<corio::Task<int>> {
            co_yield co_await corio::spawn(f(1));
            co_yield co_await corio::spawn(f(2));
            co_yield co_await corio::spawn(f(3));
        };

        auto h = [&]() -> corio::Lazy<void> {
            auto gen = g();
            std::vector<corio::Task<int>> arr;

            while (co_await gen) {
                arr.push_back(gen.current());
            }

            auto result = co_await corio::gather(arr);
            CHECK(result.size() == 3);
            CHECK(result[0].result() == 1);
            CHECK(result[1].result() == 2);
            CHECK(result[2].result() == 3);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), h());
    }

    SUBCASE("generator with move-only type") {
        auto f = []() -> corio::Generator<std::unique_ptr<int>> {
            co_yield std::make_unique<int>(1);
            co_yield std::make_unique<int>(2);
            co_yield std::make_unique<int>(3);
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto gen = f();
            std::vector<std::unique_ptr<int>> arr;

            while (co_await gen) {
                arr.push_back(gen.current());
            }

            CHECK(arr.size() == 3);
            CHECK(*arr[0] == 1);
            CHECK(*arr[1] == 2);
            CHECK(*arr[2] == 3);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("use async_for") {
        auto f = []() -> corio::Generator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };

        auto g = [&]() -> corio::Lazy<void> {
            std::vector<int> arr;
            async_for(auto e, f()) {
                arr.push_back(e);
            }
            CHECK(arr == std::vector<int>{1, 2, 3});
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }
}