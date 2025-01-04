#include <corio/any_awaitable.hpp>
#include <corio/detail/concepts.hpp>
#include <corio/lazy.hpp>
#include <corio/runner.hpp>
#include <corio/task.hpp>
#include <doctest/doctest.h>
#include <variant>
#include <vector>

TEST_CASE("test any awaitable") {
    using AnyAwaitableType =
        corio::AnyAwaitable<corio::Lazy<int>, corio::Task<int>>;
    static_assert(corio::detail::awaitable<AnyAwaitableType>);

    auto f = [&]() -> corio::Lazy<int> { co_return 42; };

    auto g = [&]() -> corio::Lazy<void> {
        AnyAwaitableType a1;
        CHECK(!a1);

        auto task = co_await corio::spawn(f());
        a1 = std::move(task);
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

    corio::run(g());
}