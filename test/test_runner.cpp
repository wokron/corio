#include "corio/this_coro.hpp"
#include <asio/thread_pool.hpp>
#include <corio/lazy.hpp>
#include <corio/run.hpp>
#include <corio/task.hpp>
#include <doctest/doctest.h>

using namespace std::chrono_literals;

namespace {

template <typename Rep, typename Period, typename Return>
inline corio::Lazy<Return> sleep(std::chrono::duration<Rep, Period> duration,
                                 Return return_value) {
    co_await corio::this_coro::sleep_for(duration);
    co_return return_value;
}

} // namespace

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
                    co_await corio::spawn(sleep(10us, i));
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

        auto start = std::chrono::high_resolution_clock::now();

        corio::block_on(pool.get_executor(), corio::this_coro::sleep_for(10us));

        auto end = std::chrono::high_resolution_clock::now();

        CHECK((end - start) >= 10us);
    }
}