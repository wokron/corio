#include <asio.hpp>
#include <corio/lazy.hpp>
#include <corio/run.hpp>
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

namespace {

#ifdef __clang__

[[clang::optnone]] std::thread::id get_tid() {
    return std::this_thread::get_id();
}

#else

std::thread::id get_tid() { return std::this_thread::get_id(); }

#endif

} // namespace

TEST_CASE("test coroutine roam") {
    SUBCASE("basic") {
        asio::thread_pool p1(1), p2(1);

        auto f = [&](asio::any_io_executor ex1,
                     asio::any_io_executor ex2) -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            CHECK(ex == ex1);
            auto id1 = get_tid();

            co_await corio::this_coro::roam_to(ex2);
            ex = co_await corio::this_coro::executor;
            CHECK(ex == ex2);
            auto id2 = get_tid();

            co_await corio::this_coro::roam_to(ex1);
            ex = co_await corio::this_coro::executor;
            CHECK(ex == ex1);
            auto id3 = get_tid();

            co_await corio::this_coro::roam_to(ex2);
            ex = co_await corio::this_coro::executor;
            CHECK(ex == ex2);
            auto id4 = get_tid();

            CHECK(id1 == id3);
            CHECK(id2 == id4);
            CHECK(id1 != id2);
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            co_await corio::this_coro::roam_to(ex); // no effect
            auto ex2 = co_await corio::this_coro::executor;
            CHECK(ex == ex2);
        };

        corio::block_on(p1.get_executor(),
                        f(p1.get_executor(), p2.get_executor()));

        corio::block_on(p1.get_executor(), g());
    }

    SUBCASE("with cancel") {
        asio::thread_pool p1(1), p2(1);

        auto f = [](asio::any_io_executor ex1,
                    asio::any_io_executor ex2) -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            REQUIRE(ex == ex1);
            while (true) {
                co_await corio::this_coro::roam_to(ex2);
                co_await corio::this_coro::yield();
                co_await corio::this_coro::roam_to(ex1);
                co_await corio::this_coro::yield();
            }
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto t =
                co_await corio::spawn(f(p1.get_executor(), p2.get_executor()));

            co_await corio::this_coro::sleep_for(500us);

            t.abort();
            CHECK_THROWS(co_await t);
        };

        corio::block_on(p1.get_executor(), g());
    }
}