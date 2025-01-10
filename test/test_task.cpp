#include "asio/thread_pool.hpp"
#include "corio/detail/defer.hpp"
#include "corio/lazy.hpp"
#include <asio.hpp>
#include <chrono>
#include <corio/task.hpp>
#include <doctest/doctest.h>
#include <thread>
#include <utility>

namespace {

struct CancelableSimpleAwaiter {
    bool await_ready() noexcept { return false; }

    CancelableSimpleAwaiter() = default;

    CancelableSimpleAwaiter(const CancelableSimpleAwaiter &) = delete;
    CancelableSimpleAwaiter &
    operator=(const CancelableSimpleAwaiter &) = delete;

    CancelableSimpleAwaiter(CancelableSimpleAwaiter &&other)
        : cancelled(std::exchange(other.cancelled, nullptr)) {}

    CancelableSimpleAwaiter &operator=(CancelableSimpleAwaiter &&other) {
        if (this != &other) {
            cancelled = std::exchange(other.cancelled, nullptr);
        }
        return *this;
    }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        CORIO_ASSERT(promise.executor(), "The executor is not set");
        auto &strand = promise.strand();
        asio::post(strand, [h = handle, c = cancelled] {
            if (!*c) {
                h.resume();
            }
        });
    }

    void await_resume() noexcept {}

    ~CancelableSimpleAwaiter() {
        if (cancelled != nullptr) {
            *cancelled = true;
        }
    }

    std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
};

} // namespace

TEST_CASE("test task") {

    SUBCASE("spawn tasks") {
        bool called = false;
        asio::thread_pool pool(2);

        auto f1 = [&]() -> corio::Lazy<void> {
            called = true;
            co_return;
        };

        auto f2 = []() -> corio::Lazy<int> { co_return 42; };

        bool called2 = false;
        auto f3 = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f1());
            co_await task;
            called2 = true;
        };

        auto t1 = corio::spawn(pool.get_executor(), f2());
        corio::spawn_background(pool.get_executor(), f3());

        pool.join();

        CHECK(called);
        CHECK(called2);
        CHECK(t1.is_finished());
        CHECK(t1.get_result().result() == 42);
    }

    SUBCASE("spawn any awaitable") {
        asio::thread_pool pool(1);

        bool called = false;

        auto f = []() -> corio::Lazy<int> { co_return 42; };

        auto g = [&]() -> corio::Lazy<void> {
            auto t = corio::spawn(pool.get_executor(), f());

            auto t2 = corio::spawn(pool.get_executor(), std::move(t));

            auto t3 =
                corio::spawn(pool.get_executor(), CancelableSimpleAwaiter{});

            CHECK(co_await t2 == 42);
            co_await t3;

            called = true;
        };

        corio::spawn_background(pool.get_executor(), g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort task basic") {
        bool called = false;
        int count = 0;
        auto f = [&]() -> corio::Lazy<void> {
            co_await CancelableSimpleAwaiter{};
            count++;
        };
        auto g = [&]() -> corio::Lazy<int> {
            while (true) {
                co_await f();
            }
            co_return 42;
        };

        SUBCASE("abort task 1") {
            auto k = [&]() -> corio::Lazy<void> {
                auto task = co_await corio::spawn(g());
                auto cancel_fn =
                    [](corio::Task<int> &task) -> corio::Lazy<void> {
                    task.abort();
                    co_return;
                };
                auto task_cancel = co_await corio::spawn(cancel_fn(task));
                co_await task_cancel;
                called = true;
            };

            asio::thread_pool pool(1); // Single thread to keep the order
            corio::spawn_background(pool.get_executor(), k());

            pool.join();

            CHECK(called);
            CHECK(count > 0);
        }

        SUBCASE("abort task 2") {
            auto k = [&]() -> corio::Lazy<void> {
                auto task = co_await corio::spawn(g());
                auto cancel_fn =
                    [](corio::Task<int> task) -> corio::Lazy<void> {
                    co_return; // task abort here
                };
                auto task_cancel =
                    co_await corio::spawn(cancel_fn(std::move(task)));
                co_await task_cancel;
                called = true;
            };

            asio::thread_pool pool(1); // Single thread to keep the order
            corio::spawn_background(pool.get_executor(), k());

            pool.join();

            CHECK(called);
            CHECK(count > 0);
        }
    }

    SUBCASE("abort already done task") {
        bool called = false;
        asio::thread_pool pool(2);

        auto f = []() -> corio::Lazy<int> {
            co_await CancelableSimpleAwaiter{};
            co_return 42;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            int r = co_await task;
            CHECK(r == 42);
            CHECK(task.is_finished());
            bool ok = task.abort();
            CHECK(!ok);
            called = true;
        };

        corio::spawn_background(pool.get_executor(), g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort multiple times") {
        bool called = false;
        asio::thread_pool pool(2);

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await CancelableSimpleAwaiter{};
            }
            co_return 42;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());

            bool ok;
            ok = task.abort();
            CHECK(ok);

            ok = task.abort();
            CHECK(!ok);

            ok = task.abort();
            CHECK(!ok);

            CHECK_THROWS_AS(co_await task, corio::CancellationError);
            CHECK(task.is_finished());
            CHECK(task.is_cancelled());
        };

        corio::spawn_background(pool.get_executor(), g());

        pool.join();
    }

    SUBCASE("abort using abort handle") {
        bool called = false;
        asio::thread_pool pool(2);

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await CancelableSimpleAwaiter{};
            }
            co_return 42;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            auto abort_handle = task.get_abort_handle();
            auto h =
                [](corio::AbortHandle<int> abort_handle) -> corio::Lazy<void> {
                bool ok = abort_handle.abort();
                CHECK(ok);
                co_return;
            };
            // AbortHandle is copiable
            auto task_cancel = co_await corio::spawn(h(abort_handle));
            CHECK_THROWS_AS(co_await task, corio::CancellationError);
            CHECK(task.is_finished());
            CHECK(task.is_cancelled());

            co_await task_cancel;
            called = true;
            auto ok = abort_handle.abort();
            CHECK(!ok);
        };

        corio::spawn_background(pool.get_executor(), g());

        pool.join();

        CHECK(called);
    }
}