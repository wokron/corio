#include <asio.hpp>
#include <corio/task.hpp>
#include <doctest/doctest.h>

namespace {

struct SimpleAwaiter {
    bool await_ready() noexcept { return false; }

    SimpleAwaiter() : cancelled(std::make_shared<bool>(false)) {}

    SimpleAwaiter(const SimpleAwaiter &) = delete;
    SimpleAwaiter &operator=(const SimpleAwaiter &) = delete;

    SimpleAwaiter(SimpleAwaiter &&other)
        : cancelled(std::exchange(other.cancelled, nullptr)) {}

    SimpleAwaiter &operator=(SimpleAwaiter &&other) {
        if (this != &other) {
            cancelled = std::exchange(other.cancelled, nullptr);
        }
        return *this;
    }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        PromiseType &promise = handle.promise();
        auto executor = promise.background()->runner.get_executor();
        asio::post(executor, [h = handle, c = cancelled] {
            if (!*c) {
                h.resume();
            }
        });
    }

    void await_resume() noexcept {}

    ~SimpleAwaiter() {
        if (cancelled != nullptr) {
            *cancelled = true;
        }
    }

    std::shared_ptr<bool> cancelled;
};

} // namespace

TEST_CASE("test task") {

    SUBCASE("spawn tasks") {
        bool called = false;
        asio::thread_pool pool(2);
        auto strand1 = asio::make_strand(pool.get_executor());
        auto strand2 = asio::make_strand(pool.get_executor());

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

        auto t1 = corio::spawn(strand1, f2());
        corio::spawn_background(strand2, f3());

        pool.join();

        CHECK(called);
        CHECK(called2);
        CHECK(t1.is_finished());
        CHECK(t1.get_result().result() == 42);
    }

    SUBCASE("spawn any awaitable") {
        asio::thread_pool pool(2);

        bool called = false;

        auto f = []() -> corio::Lazy<int> { co_return 42; };

        auto g = [&]() -> corio::Lazy<void> {
            auto t = corio::spawn(pool.get_executor(), f());

            auto t2 = corio::spawn(pool.get_executor(), std::move(t));

            auto t3 = corio::spawn(pool.get_executor(), SimpleAwaiter{});

            CHECK(co_await t2 == 42);
            co_await t3;

            called = true;
        };

        corio::spawn_background(asio::make_strand(pool.get_executor()), g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort task basic") {
        bool called = false;
        asio::thread_pool pool(2);
        auto strand = asio::make_strand(pool.get_executor());

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await SimpleAwaiter{};
            }
            co_return 42;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            bool ok = task.abort();
            CHECK(ok);
            CHECK_THROWS_AS(co_await task, corio::CancellationError);
            CHECK(task.is_finished());
            CHECK(task.is_cancelled());
            called = true;
        };

        corio::spawn_background(strand, g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort task cascade") {
        bool called = false;
        asio::thread_pool pool(2);
        auto strand = asio::make_strand(pool.get_executor());

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await SimpleAwaiter{};
            }
            co_return 42;
        };
        auto g = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());
            auto task2 = co_await corio::spawn(std::move(task));
            auto task3 = co_await corio::spawn(std::move(task2));
            bool ok = task3.abort();
            CHECK(ok);
            CHECK_THROWS_AS(co_await task3, corio::CancellationError);
            CHECK(task3.is_finished());
            CHECK(task3.is_cancelled());
            called = true;
        };

        corio::spawn_background(strand, g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort already done task") {
        bool called = false;
        asio::thread_pool pool(2);

        auto f = []() -> corio::Lazy<int> {
            co_await SimpleAwaiter{};
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
        auto strand = asio::make_strand(pool.get_executor());

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await SimpleAwaiter{};
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
            called = true;
        };

        corio::spawn_background(strand, g());

        pool.join();

        CHECK(called);
    }

    SUBCASE("abort using abort handle") {
        bool called = false;
        asio::thread_pool pool(2);
        auto strand = asio::make_strand(pool.get_executor());

        auto f = []() -> corio::Lazy<int> {
            while (true) {
                co_await SimpleAwaiter{};
            }
            co_return 42;
        };
        auto g = [](corio::AbortHandle<int> handle) -> corio::Lazy<bool> {
            bool ok = handle.abort();
            co_return ok;
        };
        auto h = [&]() -> corio::Lazy<void> {
            auto task = co_await corio::spawn(f());

            std::vector<corio::Task<bool>> tasks;
            for (int i = 0; i < 5; ++i) {
                tasks.push_back(
                    co_await corio::spawn(g(task.get_abort_handle())));
            }

            CHECK_THROWS_AS(co_await task, corio::CancellationError);
            CHECK(task.is_finished());
            CHECK(task.is_cancelled());

            int ok_times = 0;
            for (auto &t : tasks) {
                if (co_await t) {
                    ++ok_times;
                }
            }
            CHECK(ok_times == 1);

            called = true;
        };

        corio::spawn_background(strand, h());

        pool.join();

        CHECK(called);
    }
}