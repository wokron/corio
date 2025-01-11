#include "asio/thread_pool.hpp"
#include "corio/lazy.hpp"
#include <asio.hpp>
#include <corio/task.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>

using namespace std::chrono_literals;

TEST_CASE("test this_coro") {
    bool called = false;
    auto f = [&]() -> corio::Lazy<asio::any_io_executor> {
        co_return co_await corio::this_coro::executor;
    };

    auto g = [&]() -> corio::Lazy<void> {
        auto ex = co_await corio::this_coro::executor;
        auto ex2 = co_await f();
        auto t = co_await corio::spawn(f());
        auto ex3 = co_await t;
        CHECK(ex == ex2);
        CHECK(ex != ex3);
        called = true;
    };

    asio::thread_pool pool(2);
    auto strand = asio::make_strand(pool.executor());

    corio::spawn_background(strand, g());

    pool.join();

    CHECK(called);
}

TEST_CASE("test this_coro awaitable") {

    SUBCASE("multi yield") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            for (int i = 0; i < 5; i++) {
                co_await corio::this_coro::yield();
            }
            called = true;
        };

        asio::thread_pool pool(1);

        corio::spawn_background(pool.executor(), f());

        pool.join();

        CHECK(called);
    }

    SUBCASE("yield with cancel") {
        bool called = false;
        int count = 0;
        auto f = [&]() -> corio::Lazy<void> {
            for (int i = 0; i < 5; i++) {
                co_await corio::this_coro::yield();
                count++;
            }
            called = true;
            co_return;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            co_await corio::this_coro::yield();
            co_await corio::this_coro::yield();
            task.abort();
            co_await task;
        };

        asio::thread_pool pool(1);

        corio::spawn_background(pool.executor(), g());

        pool.join();

        CHECK(count > 0);
        CHECK_FALSE(called);
    }

    SUBCASE("test sleep for") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            co_await corio::this_coro::sleep_for(100us);
            called = true;
        };

        asio::thread_pool pool(1);

        corio::spawn_background(pool.executor(), f());

        auto start = std::chrono::steady_clock::now();

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(called);
        CHECK(duration >= 100us);
    }

    SUBCASE("test sleep until") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            auto now_time = std::chrono::steady_clock::now();
            co_await corio::this_coro::sleep_until(now_time + 100us);
            called = true;
        };

        asio::thread_pool pool(1);

        corio::spawn_background(pool.executor(), f());

        auto start = std::chrono::steady_clock::now();

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(called);
        CHECK(duration >= 100us);
    }

    SUBCASE("test sleep concurrent") {
        auto g = [](std::chrono::microseconds duration,
                    int i) -> corio::Lazy<int> {
            co_await corio::this_coro::sleep_for(duration);
            co_return i;
        };

        auto f = [&]() -> corio::Lazy<void> {
            std::vector<corio::Task<int>> tasks;
            for (int i = 0; i < 5; i++) {
                tasks.push_back(co_await corio::spawn(g(100us, i)));
            }
            for (int i = 0; i < 5; i++) {
                co_await tasks[i];
            }
        };

        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();

        corio::spawn_background(pool.executor(), f());

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(100us <= duration);
        CHECK(duration <= 250us);
    }

    SUBCASE("test sleep with cancel") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            co_await corio::this_coro::sleep_for(1ms);
            called = true;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            co_await corio::this_coro::yield();
            co_await corio::this_coro::yield();
            task.abort();
            co_await task;
        };

        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();

        corio::spawn_background(pool.executor(), g());

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(!called);
        CHECK(duration < 1ms);
    }
}