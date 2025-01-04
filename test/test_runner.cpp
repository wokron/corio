#include "corio/this_coro.hpp"
#include <asio/thread_pool.hpp>
#include <corio/lazy.hpp>
#include <corio/runner.hpp>
#include <corio/task.hpp>
#include <doctest/doctest.h>

using namespace std::chrono_literals;

TEST_CASE("test runner") {
    SUBCASE("test block_no") {
        auto f = []() -> corio::Lazy<int> { co_return 42; };

        asio::thread_pool pool(1);

        auto result = corio::block_on(pool.get_executor(), f());
        CHECK(result == 42);
    }

    SUBCASE("test run") {
        auto f = []() -> corio::Lazy<int> {
            int sum = 0;
            std::vector<corio::Task<int>> tasks;
            for (int i = 0; i < 16; i++) {
                auto task =
                    co_await corio::spawn(corio::this_coro::sleep(10us, i));
                tasks.push_back(std::move(task));
            }
            for (auto &task : tasks) {
                sum += co_await task;
            }
            co_return sum;
        };

        auto result = corio::run(f());
        CHECK(result == 120);
    }

    SUBCASE("test block_no with common awaiter") {
        asio::thread_pool pool(1);

        auto sleep = corio::this_coro::sleep(10us);

        auto start = std::chrono::high_resolution_clock::now();

        corio::block_on(pool.get_executor(), sleep);
        corio::block_on(pool.get_executor(), sleep);
        corio::block_on(pool.get_executor(), std::move(sleep));

        auto end = std::chrono::high_resolution_clock::now();

        CHECK((end - start) >= 30us);
    }
}