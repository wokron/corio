#include <asio.hpp>
#include <corio/lazy.hpp>
#include <corio/run.hpp>
#include <corio/task.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>
#include <vector>

namespace {

template <typename Rep, typename Period, typename Return>
inline corio::Lazy<Return> sleep(std::chrono::duration<Rep, Period> duration,
                                 Return return_value) {
    co_await corio::this_coro::sleep_for(duration);
    co_return return_value;
}

} // namespace

using namespace std::chrono_literals;

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

        corio::spawn(pool.executor(), f());

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

        corio::spawn(pool.executor(), g());

        pool.join();

        CHECK(count > 0);
        CHECK_FALSE(called);
    }

    SUBCASE("test sleep") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            co_await corio::this_coro::sleep_for(100us);
            called = true;
        };

        asio::thread_pool pool(1);

        corio::spawn(pool.executor(), f());

        auto start = std::chrono::steady_clock::now();

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(called);
        CHECK(duration >= 100us);
    }

    SUBCASE("test sleep concurrent") {
        auto f = [&]() -> corio::Lazy<void> {
            std::vector<corio::Task<int>> tasks;
            for (int i = 0; i < 5; i++) {
                tasks.push_back(co_await corio::spawn(sleep(100us, i)));
            }
            for (int i = 0; i < 5; i++) {
                co_await tasks[i];
            }
        };

        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();

        corio::spawn(pool.executor(), f());

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

        corio::spawn(pool.executor(), g());

        pool.join();

        auto end = std::chrono::steady_clock::now();
        auto duration = end - start;

        CHECK(!called);
        CHECK(duration < 1ms);
    }
}

[[clang::optnone]] std::thread::id get_tid() {
    return std::this_thread::get_id();
}

TEST_CASE("test this_coro run_on") {
    asio::thread_pool pool1(1), pool2(1);
    bool called = false;
    auto f = [&]() -> corio::Lazy<void> {
        auto id1 = get_tid();
        co_await corio::this_coro::run_on(pool2.get_executor());
        auto id2 = get_tid();
        co_await corio::this_coro::run_on(pool1.get_executor());
        auto id3 = get_tid();
        co_await corio::this_coro::run_on(pool2.get_executor());
        auto id4 = get_tid();
        CHECK(id1 != id2);
        CHECK(id1 == id3);
        CHECK(id2 == id4);
        called = true;
    };

    corio::block_on(pool1.get_executor(), f());

    CHECK(called);
}