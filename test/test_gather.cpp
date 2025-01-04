#include "asio/thread_pool.hpp"
#include "corio/runner.hpp"
#include <corio/detail/defer.hpp>
#include <corio/gather.hpp>
#include <corio/lazy.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>
#include <functional>

using namespace std::chrono_literals;

TEST_CASE("test gather") {

    SUBCASE("gather basic") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };
        auto g = []() -> corio::Lazy<int> { co_return 2; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };
        bool called = false;

        auto entry = corio::gather(f(), g(), h(called));

        asio::thread_pool pool(1);
        auto [a, b, c] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(a == 1);
        CHECK(b == 2);
        CHECK(called);
    }

    SUBCASE("gather time") {
        auto entry = corio::gather(corio::this_coro::sleep(100us),
                                   corio::this_coro::sleep(200us),
                                   corio::this_coro::sleep(300us));
        asio::thread_pool pool(1);
        auto start = std::chrono::steady_clock::now();
        corio::block_on(pool.get_executor(), std::move(entry));
        auto end = std::chrono::steady_clock::now();
        CHECK((end - start) >= 300us);
    }

    SUBCASE("gather exception") {
        auto f = []() -> corio::Lazy<int> {
            throw std::runtime_error("error");
            co_return 1;
        };

        auto sleep = corio::this_coro::sleep(300us);

        auto entry = corio::gather(f(), sleep);
        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();
        CHECK_THROWS_AS(corio::block_on(pool.get_executor(), std::move(entry)),
                        std::runtime_error);
        auto end = std::chrono::steady_clock::now();
        CHECK((end - start) < 250us);

        auto start2 = std::chrono::steady_clock::now();
        corio::block_on(pool.get_executor(), std::move(sleep));
        auto end2 = std::chrono::steady_clock::now();
        CHECK((end2 - start2) >= 300us);
    }

    SUBCASE("gather move-only type") {
        auto f = []() -> corio::Lazy<std::unique_ptr<int>> {
            co_return std::make_unique<int>(1);
        };
        auto g = []() -> corio::Lazy<std::unique_ptr<double>> {
            co_return std::make_unique<double>(2.34);
        };
        bool called = false;

        auto entry = corio::gather(f(), g());

        asio::thread_pool pool(1);
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(*a == 1);
        CHECK(*b == 2.34);
    }

    SUBCASE("gather cancel") {
        auto f = [&](bool &called) -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&] { called = true; });
            co_await corio::this_coro::sleep(1s);
        };

        auto g = [&]() -> corio::Lazy<void> {
            bool called1 = false;
            bool called2 = false;
            auto t1 = co_await corio::spawn(f(called1));
            auto t2 = co_await corio::spawn(f(called2));
            t2.set_abort_guard(true);
            auto exception_lazy = []() -> corio::Lazy<void> {
                throw std::runtime_error("error");
                co_return;
            };
            CHECK_THROWS(co_await corio::gather(t1, t2, exception_lazy()));
            CHECK(!called1);
            CHECK(!called2);

            CHECK_THROWS(
                co_await corio::gather(t1, std::move(t2), exception_lazy()));
            co_await corio::this_coro::yield();
            CHECK(!called1);
            CHECK(called2);

            CHECK_THROWS(
                co_await corio::gather(std::move(t1), exception_lazy()));
            co_await corio::this_coro::yield();
            CHECK(!called1); // t1 is detached, but not canceled
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }
}