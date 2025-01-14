#include <asio.hpp>
#include <corio/any_awaitable.hpp>
#include <corio/detail/defer.hpp>
#include <corio/gather.hpp>
#include <corio/lazy.hpp>
#include <corio/operators.hpp>
#include <corio/run.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>
#include <functional>
#include <variant>
#include <vector>

using namespace std::chrono_literals;

TEST_CASE("test try gather") {

    SUBCASE("gather basic") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };
        auto g = []() -> corio::Lazy<int> { co_return 2; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };
        bool called = false;

        auto entry = corio::try_gather(f(), g(), h(called));

        asio::thread_pool pool(1);
        auto [a, b, c] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(a == 1);
        CHECK(b == 2);
        CHECK(called);
    }

    SUBCASE("gather time") {
        auto entry = corio::try_gather(corio::this_coro::sleep_for(100us),
                                       corio::this_coro::sleep_for(200us),
                                       corio::this_coro::sleep_for(300us));
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

        auto sleep = corio::this_coro::sleep_for(300us);

        auto entry = corio::try_gather(f(), std::move(sleep));
        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();
        CHECK_THROWS_AS(corio::block_on(pool.get_executor(), std::move(entry)),
                        std::runtime_error);
        auto end = std::chrono::steady_clock::now();
        CHECK((end - start) < 250us);

        auto sleep2 = corio::this_coro::sleep_for(300us);

        auto start2 = std::chrono::steady_clock::now();
        corio::block_on(pool.get_executor(), std::move(sleep2));
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

        auto entry = corio::try_gather(f(), g());

        asio::thread_pool pool(1);
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(*a == 1);
        CHECK(*b == 2.34);
    }

    SUBCASE("gather void type") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        bool called = false;
        auto entry =
            corio::try_gather(f(called), corio::this_coro::sleep_for(100us));
        asio::thread_pool pool(1);

        auto [x, y] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(x == std::monostate{});
        CHECK(y == std::monostate{});

        CHECK(called);
    }

    SUBCASE("gather cancel") {
        auto f = [&](bool &called) -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&] { called = true; });
            co_await corio::this_coro::sleep_for(1s);
        };

        auto g = [&]() -> corio::Lazy<void> {
            bool called1 = false;
            bool called2 = false;
            auto t1 = co_await corio::spawn(f(called1));
            auto t2 = co_await corio::spawn(f(called2));
            auto exception_lazy = []() -> corio::Lazy<void> {
                throw std::runtime_error("error");
                co_return;
            };
            CHECK_THROWS(co_await corio::try_gather(t1, t2, exception_lazy()));
            CHECK(!called1);
            CHECK(!called2);

            CHECK_THROWS(co_await corio::try_gather(t1, std::move(t2),
                                                    exception_lazy()));
            co_await corio::this_coro::do_yield();
            CHECK(!called1);
            CHECK(called2);

            CHECK_THROWS(
                co_await corio::try_gather(std::move(t1), exception_lazy()));
            co_await corio::this_coro::do_yield();
            CHECK(called1);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }
}

TEST_CASE("test try gather iter") {
    SUBCASE("gather iter basic") {
        auto f = [](int v) -> corio::Lazy<int> { co_return v; };

        std::array<corio::Lazy<int>, 3> arr;
        arr[0] = f(1);
        arr[1] = f(2);
        arr[2] = f(3);

        auto entry = corio::try_gather(arr);

        asio::thread_pool pool(1);
        auto result = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(result.size() == 3);
        CHECK(result[0] == 1);
        CHECK(result[1] == 2);
        CHECK(result[2] == 3);
    }

    SUBCASE("gather iter with exception") {
        auto f = [](int v) -> corio::Lazy<int> {
            if (v == 2) {
                throw std::runtime_error("error");
            }
            co_return v;
        };

        std::vector<corio::Lazy<int>> vec;
        vec.push_back(f(1));
        vec.push_back(f(2));
        vec.push_back(f(3));

        auto entry = corio::try_gather(vec);

        asio::thread_pool pool(1);
        CHECK_THROWS_AS(corio::block_on(pool.get_executor(), std::move(entry)),
                        std::runtime_error);
    }

    SUBCASE("gather iter with move-only type") {
        auto f = [](int v) -> corio::Lazy<std::unique_ptr<int>> {
            co_return std::make_unique<int>(v);
        };

        std::vector<corio::Lazy<std::unique_ptr<int>>> vec;
        vec.push_back(f(1));
        vec.push_back(f(2));
        vec.push_back(f(3));

        auto entry = corio::try_gather(vec);

        asio::thread_pool pool(1);
        auto result = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(result.size() == 3);
        CHECK(*result[0] == 1);
        CHECK(*result[1] == 2);
        CHECK(*result[2] == 3);
    }

    SUBCASE("gather iter with void type") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        bool called = false;
        bool called2 = false;
        std::vector<corio::Lazy<void>> vec;
        vec.push_back(f(called));
        vec.push_back(f(called2));

        auto entry = corio::try_gather(vec);
        asio::thread_pool pool(1);

        auto result = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(result.size() == 2);
        CHECK(result[0] == std::monostate{});
        CHECK(result[1] == std::monostate{});
        CHECK(called);
        CHECK(called2);
    }

    SUBCASE("gather iter with cancel") {
        auto f = [&](bool &called) -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&] { called = true; });
            co_await corio::this_coro::sleep_for(1s);
        };

        auto g = [&]() -> corio::Lazy<void> {
            bool called1 = false;
            bool called2 = false;
            auto t1 = co_await corio::spawn(f(called1));
            auto t2 = co_await corio::spawn(f(called2));
            auto exception_lazy = []() -> corio::Lazy<void> {
                throw std::runtime_error("error");
                co_return;
            };

            std::vector<corio::Task<void>> vec;
            vec.push_back(std::move(t1));
            vec.push_back(std::move(t2));
            vec.push_back(co_await corio::spawn(exception_lazy()));

            CHECK_THROWS(co_await corio::try_gather(vec));
            CHECK(!called1);
            CHECK(!called2);

            vec[2] = co_await corio::spawn(exception_lazy());

            CHECK_THROWS(co_await corio::try_gather(std::move(vec)));
            co_await corio::this_coro::do_yield();
            CHECK(called1);
            CHECK(called2);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("gather with any awaitable") {
        using T = corio::AnyAwaitable<void, int, double>;
        auto f = []() -> corio::Lazy<int> { co_return 42; };
        auto g = []() -> corio::Lazy<double> { co_return 2.34; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        auto main = [&]() -> corio::Lazy<void> {
            bool called = false;
            std::vector<T> vec;
            vec.push_back(co_await corio::spawn(f()));
            vec.push_back(co_await corio::spawn(g()));
            vec.push_back(co_await corio::spawn(h(called)));

            auto result = co_await corio::try_gather(vec);
            CHECK(result.size() == 3);
            CHECK(std::get<int>(result[0]) == 42);
            CHECK(std::get<double>(result[1]) == 2.34);
            CHECK(called);
            CHECK(std::holds_alternative<std::monostate>(result[2]));
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), main());
    }
}

TEST_CASE("test try gather operator") {
    using namespace corio::awaitable_operators;

    SUBCASE("gather basic") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };
        auto g = []() -> corio::Lazy<int> { co_return 2; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };
        bool called = false;

        auto entry = f() && g() && h(called);

        asio::thread_pool pool(1);
        auto [a, b, c] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(a == 1);
        CHECK(b == 2);
        CHECK(called);
    }

    SUBCASE("gather time") {
        auto entry = corio::this_coro::sleep_for(100us) &&
                     corio::this_coro::sleep_for(200us) &&
                     corio::this_coro::sleep_for(300us);
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

        auto sleep = corio::this_coro::sleep_for(300us);

        auto entry = f() && std::move(sleep);
        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();
        CHECK_THROWS_AS(corio::block_on(pool.get_executor(), std::move(entry)),
                        std::runtime_error);
        auto end = std::chrono::steady_clock::now();
        CHECK((end - start) < 250us);

        auto sleep2 = corio::this_coro::sleep_for(300us);

        auto start2 = std::chrono::steady_clock::now();
        corio::block_on(pool.get_executor(), std::move(sleep2));
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

        auto entry = f() && g();

        asio::thread_pool pool(1);
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(*a == 1);
        CHECK(*b == 2.34);
    }

    SUBCASE("gather void type") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        bool called = false;
        auto entry = f(called) && corio::this_coro::sleep_for(100us);
        asio::thread_pool pool(1);

        auto [x, y] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(x == std::monostate{});
        CHECK(y == std::monostate{});

        CHECK(called);
    }

    SUBCASE("gather cancel") {
        auto f = [&](bool &called) -> corio::Lazy<void> {
            corio::detail::DeferGuard guard([&] { called = true; });
            co_await corio::this_coro::sleep_for(1s);
        };

        auto g = [&]() -> corio::Lazy<void> {
            bool called1 = false;
            bool called2 = false;
            auto t1 = co_await corio::spawn(f(called1));
            auto t2 = co_await corio::spawn(f(called2));
            auto exception_lazy = []() -> corio::Lazy<void> {
                throw std::runtime_error("error");
                co_return;
            };

            CHECK_THROWS(co_await (t1 && t2 && exception_lazy()));
            CHECK(!called1);
            CHECK(!called2);

            CHECK_THROWS(co_await (t1 && std::move(t2) && exception_lazy()));
            co_await corio::this_coro::do_yield();
            CHECK(!called1);
            CHECK(called2);

            CHECK_THROWS(co_await (std::move(t1) && exception_lazy()));
            co_await corio::this_coro::do_yield();
            CHECK(called1);
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), g());
    }
}

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
        CHECK(a.result() == 1);
        CHECK(b.result() == 2);
        CHECK(called);
        CHECK_NOTHROW(c.result());
    }

    SUBCASE("gather exception") {
        auto f = []() -> corio::Lazy<int> {
            throw std::runtime_error("error");
            co_return 1;
        };

        auto sleep = corio::this_coro::sleep_for(300us);

        auto entry = corio::gather(f(), sleep);
        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK_THROWS_AS(a.result(), std::runtime_error);
        CHECK_NOTHROW(b.result());
        auto end = std::chrono::steady_clock::now();

        CHECK((end - start) >= 300us);
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
        CHECK(*a.result() == 1);
        CHECK(*b.result() == 2.34);
    }

    SUBCASE("gather void type") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        bool called = false;
        auto entry =
            corio::gather(f(called), corio::this_coro::sleep_for(100us));
        asio::thread_pool pool(1);

        auto [x, y] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK_NOTHROW(x.result());
        CHECK_NOTHROW(y.result());

        CHECK(called);
    }
}

TEST_CASE("test gather iter") {
    SUBCASE("gather iter basic") {
        auto f = [](int v) -> corio::Lazy<int> { co_return v; };

        std::array<corio::Lazy<int>, 3> arr;
        arr[0] = f(1);
        arr[1] = f(2);
        arr[2] = f(3);

        auto entry = corio::gather(arr);

        asio::thread_pool pool(1);
        auto result = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(result.size() == 3);
        CHECK(result[0].result() == 1);
        CHECK(result[1].result() == 2);
        CHECK(result[2].result() == 3);
    }

    SUBCASE("gather iter with exception") {
        auto f = [](int v) -> corio::Lazy<int> {
            if (v == 2) {
                throw std::runtime_error("error");
            }
            co_return v;
        };

        std::vector<corio::Lazy<int>> vec;
        vec.push_back(f(1));
        vec.push_back(f(2));
        vec.push_back(f(3));

        auto entry = corio::gather(vec);

        asio::thread_pool pool(1);
        auto result = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(result.size() == 3);
        CHECK(result[0].result() == 1);
        CHECK_THROWS_AS(result[1].result(), std::runtime_error);
        CHECK(result[2].result() == 3);
    }

    SUBCASE("gather with any awaitable") {
        using T = corio::AnyAwaitable<void, int, double>;
        auto f = []() -> corio::Lazy<int> { co_return 42; };
        auto g = []() -> corio::Lazy<double> { co_return 2.34; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        auto main = [&]() -> corio::Lazy<void> {
            bool called = false;
            std::vector<T> vec;
            vec.push_back(co_await corio::spawn(f()));
            vec.push_back(co_await corio::spawn(g()));
            vec.push_back(co_await corio::spawn(h(called)));

            auto result = co_await corio::gather(vec);
            CHECK(result.size() == 3);
            CHECK(std::get<int>(result[0].result()) == 42);
            CHECK(std::get<double>(result[1].result()) == 2.34);
            CHECK(called);
            CHECK(std::holds_alternative<std::monostate>(result[2].result()));
        };

        asio::thread_pool pool(1);
        corio::block_on(pool.get_executor(), main());
    }
}

TEST_CASE("test gather operator") {
    using namespace corio::awaitable_operators;

    SUBCASE("gather basic") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };
        auto g = []() -> corio::Lazy<int> { co_return 2; };
        auto h = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };
        bool called = false;

        auto entry = f() & g() & h(called);

        asio::thread_pool pool(1);
        auto [a, b, c] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(a.result() == 1);
        CHECK(b.result() == 2);
        CHECK(called);
        CHECK_NOTHROW(c.result());
    }

    SUBCASE("gather exception") {
        auto f = []() -> corio::Lazy<int> {
            throw std::runtime_error("error");
            co_return 1;
        };

        auto sleep = corio::this_coro::sleep_for(300us);

        auto entry = f() & std::move(sleep);
        asio::thread_pool pool(1);

        auto start = std::chrono::steady_clock::now();
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK_THROWS_AS(a.result(), std::runtime_error);
        CHECK_NOTHROW(b.result());
        auto end = std::chrono::steady_clock::now();

        CHECK((end - start) >= 300us);
    }

    SUBCASE("gather move-only type") {
        auto f = []() -> corio::Lazy<std::unique_ptr<int>> {
            co_return std::make_unique<int>(1);
        };
        auto g = []() -> corio::Lazy<std::unique_ptr<double>> {
            co_return std::make_unique<double>(2.34);
        };
        bool called = false;

        auto entry = f() & g();

        asio::thread_pool pool(1);
        auto [a, b] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK(*a.result() == 1);
        CHECK(*b.result() == 2.34);
    }

    SUBCASE("gather void type") {
        auto f = [](bool &called) -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        bool called = false;
        auto entry = f(called) & corio::this_coro::sleep_for(100us);
        asio::thread_pool pool(1);

        auto [x, y] = corio::block_on(pool.get_executor(), std::move(entry));
        CHECK_NOTHROW(x.result());
        CHECK_NOTHROW(y.result());

        CHECK(called);
    }
}