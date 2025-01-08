#include "asio/thread_pool.hpp"
#include "corio/lazy.hpp"
#include <array>
#include <chrono>
#include <corio/any_awaitable.hpp>
#include <corio/detail/defer.hpp>
#include <corio/run.hpp>
#include <corio/select.hpp>
#include <corio/task.hpp>
#include <corio/this_coro.hpp>
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

TEST_CASE("test select") {

    SUBCASE("select basic") {
        auto f = [&]() -> corio::Lazy<void> {
            auto start = std::chrono::steady_clock::now();
            auto r = co_await corio::select(sleep(1s, 1),
                                            sleep(500us, 2),
                                            corio::this_coro::sleep_for(10s));
            auto end = std::chrono::steady_clock::now();
            CHECK(r.index() == 1);
            CHECK(std::get<1>(r) == 2);
            CHECK((end - start) < 700us);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());
    }

    SUBCASE("select cancel task") {
        bool called = false;
        auto f = [&]() -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&]() { called = true; });
            co_await corio::this_coro::sleep_for(1s);
            CHECK(false);
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto t = co_await corio::spawn(f());
            t.set_abort_guard(true);
            auto r = co_await corio::select(corio::this_coro::sleep_for(100us),
                                            std::move(t));
            CHECK(r.index() == 0);
            co_await corio::this_coro::yield();
            CHECK(called);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("select with exception") {
        auto f = []() -> corio::Lazy<void> {
            throw std::runtime_error("error");
            co_return;
        };

        auto g = [&]() -> corio::Lazy<void> {
            auto start = std::chrono::steady_clock::now();
            CHECK_THROWS_AS(co_await corio::select(corio::this_coro::sleep_for(1s),
                                                   co_await corio::spawn(f())),
                            std::runtime_error);
            auto end = std::chrono::steady_clock::now();
            CHECK((end - start) < 1s);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("select move-only type") {
        auto f = []() -> corio::Lazy<std::unique_ptr<int>> {
            co_return std::make_unique<int>(1);
        };

        auto g = []() -> corio::Lazy<std::unique_ptr<double>> {
            co_await corio::this_coro::sleep_for(1s);
            co_return std::make_unique<double>(2.34);
        };

        auto h = []() -> corio::Lazy<int> {
            co_await corio::this_coro::sleep_for(1s);
            co_return 42;
        };

        auto l = [&]() -> corio::Lazy<void> {
            auto r = co_await corio::select(f(), g(), h());
            CHECK(r.index() == 0);
            CHECK(*std::get<0>(r) == 1);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), l());
    }
}

TEST_CASE("test select iter") {

    SUBCASE("select basic") {
        auto f = [&]() -> corio::Lazy<void> {
            std::vector<corio::Lazy<int>> vec;
            vec.push_back(sleep(1s, 1));
            vec.push_back(sleep(100us, 2));

            auto start = std::chrono::steady_clock::now();
            auto [no, r] = co_await corio::select(vec);
            auto end = std::chrono::steady_clock::now();
            CHECK(no == 1);
            CHECK(r == 2);
            CHECK((end - start) < 200us);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());
    }

    SUBCASE("select multiple tasks") {
        auto f = [](int v, std::chrono::microseconds us) -> corio::Lazy<int> {
            co_await corio::this_coro::sleep_for(us);
            co_return v;
        };

        auto g = [&]() -> corio::Lazy<void> {
            std::vector<corio::Task<int>> vec;
            for (int i = 0; i < 10; i++) {
                auto time = 100us + 100us * i;
                auto task = co_await corio::spawn(f(i, time));
                if (i % 3 == 0) {
                    vec.push_back(std::move(task));
                } else {
                    vec.insert(vec.begin(), std::move(task));
                }
            }

            int count = 0;
            while (!vec.empty()) {
                auto [no, r] = co_await corio::select(vec);
                CHECK(r == count); // Tasks finish in order
                vec.erase(vec.begin() + no);
                count++;
            }
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("select multiple cancecl") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&]() { called = true; });
            co_await corio::this_coro::sleep_for(1s);
        };

        auto g = [&]() -> corio::Lazy<void> {
            using T = corio::AnyAwaitable<void>;

            std::vector<T> vec;
            std::array<bool, 10> flags;
            for (int i = 0; i < 10; i++) {
                auto task = co_await corio::spawn(f(flags[i]));
                task.set_abort_guard(true);
                vec.push_back(std::move(task));
            }

            vec.push_back(corio::this_coro::sleep_for(100us));

            auto r = co_await corio::select(std::move(vec));
            CHECK(r.first == 10); // Sleep task finishes first
            co_await corio::this_coro::yield();
            for (auto &&flag : flags) {
                CHECK(flag);
            }
        };
    }
}