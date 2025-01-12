#include <asio.hpp>
#include <chrono>
#include <corio/run.hpp>
#include <doctest/doctest.h>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("test run") {
    SUBCASE("test block_on") {
        auto f = []() -> corio::Lazy<int> { co_return 42; };

        asio::thread_pool pool(1);

        auto result = corio::block_on(pool.get_executor(), f());
        CHECK(result == 42);
    }

    SUBCASE("test run") {
        auto f = [](int i) -> corio::Lazy<int> {
            std::this_thread::sleep_for(50us); // cpu-bound
            co_return i;
        };
        auto g = [&]() -> corio::Lazy<int> {
            int sum = 0;
            std::vector<corio::Task<int>> tasks;
            for (int i = 0; i < 16; i++) {
                auto task = co_await corio::spawn(f(i));
                tasks.push_back(std::move(task));
            }
            for (auto &task : tasks) {
                sum += co_await task;
            }
            co_return sum;
        };

        SUBCASE("test run multi thread") {
            auto start = std::chrono::steady_clock::now();
            auto result = corio::run(g());
            auto end = std::chrono::steady_clock::now();
            CHECK(result == 120);
        }

        SUBCASE("test run single thread") {
            auto start = std::chrono::steady_clock::now();
            auto result = corio::run(g(), /*multi_thread=*/false);
            auto end = std::chrono::steady_clock::now();
            CHECK(result == 120);
        }
    }

    SUBCASE("test block_on with common awaiter") {
        auto f = []() -> corio::Lazy<int> { co_return 42; };

        asio::thread_pool pool(1);

        auto task = corio::spawn(pool.get_executor(), f());

        auto r = corio::block_on(pool.get_executor(), std::move(task));

        CHECK(r == 42);
    }
}