#include <asio.hpp>
#include <corio/any_awaitable.hpp>
#include <corio/detail/concepts.hpp>
#include <corio/lazy.hpp>
#include <corio/runner.hpp>
#include <corio/task.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>
#include <type_traits>
#include <variant>
#include <vector>

TEST_CASE("test any awaitable") {

    SUBCASE("any awaitable basic") {
        using AnyAwaitableType =
            corio::AnyAwaitable<corio::Lazy<int>, corio::Task<int>>;
        static_assert(corio::detail::awaitable<AnyAwaitableType>);

        auto f = [&]() -> corio::Lazy<int> { co_return 42; };

        auto g = [&]() -> corio::Lazy<void> {
            AnyAwaitableType a1;
            CHECK(!a1);

            a1 = co_await corio::spawn(f());
            CHECK(a1);

            auto lazy = f();
            AnyAwaitableType a2(std::move(lazy));
            CHECK(a2);

            std::vector<AnyAwaitableType> v;
            v.push_back(std::move(a1));
            v.push_back(std::move(a2));

            for (auto &a : v) {
                auto result = co_await a;
                CHECK(result == 42);
            }

            CHECK(std::holds_alternative<corio::Task<int>>(v[0].unwrap()));
            CHECK(!v[0]);
            CHECK(std::holds_alternative<corio::Lazy<int>>(v[1].unwrap()));
            CHECK(!v[1]);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("all void") {
        using AnyAwaitableType =
            corio::AnyAwaitable<corio::Lazy<void>,
                                decltype(corio::this_coro::yield())>;
        static_assert(corio::detail::awaitable<AnyAwaitableType>);

        auto f = [&]() -> corio::Lazy<void> { co_return; };
        AnyAwaitableType a1 = f();
        AnyAwaitableType a2 = corio::this_coro::yield();

        bool called = false;
        auto g = [&]() -> corio::Lazy<void> {
            std::vector<AnyAwaitableType> v;
            v.push_back(std::move(a1));
            v.push_back(std::move(a2));

            for (auto &a : v) {
                co_await a;
            }
            called = true;
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
        CHECK(called);
    }

    SUBCASE("difference return type") {
        using AnyAwaitableType =
            corio::AnyAwaitable<corio::Lazy<int>, corio::Lazy<double>,
                                corio::Task<double>>;
        static_assert(corio::detail::awaitable<AnyAwaitableType>);

        auto f = [&]() -> corio::Lazy<int> { co_return 42; };
        auto g = [&]() -> corio::Lazy<double> { co_return 3.14; };
        auto h = [&]() -> corio::Lazy<void> {
            AnyAwaitableType a1 = f();
            AnyAwaitableType a2 = g();

            auto r1 = co_await a1;
            CHECK(std::get<int>(r1) == 42);

            auto r2 = co_await a2;
            CHECK(std::get<double>(r2) == 3.14);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), h());
    }
}