#include <corio/lazy.hpp>
#include <doctest/doctest.h>
#include <memory>
#include <thread>

namespace {

struct SimpleAwaiter {
    bool await_ready() noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        corio::handle_resume(handle);
    }

    void await_resume() noexcept {}
};

struct GatherAwaiter {
    bool await_ready() noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        auto callback =
            [](corio::Lazy<int> &lazy, int &count,
               std::coroutine_handle<PromiseType> handle) -> corio::Lazy<void> {
            int r = co_await lazy;
            count--;
            if (count == 0) {
                corio::handle_resume(handle);
            }
        };

        count = lazys.size();
        for (auto &lazy : lazys) {
            auto entry = callback(lazy, count, handle);
            entry.set_strand(handle.promise().strand());
            entry.execute();
            auto h = entry.release();
            h.promise().set_destroy_when_exit(true);
        }
    }

    std::vector<int> await_resume() noexcept {
        std::vector<int> results;
        for (auto &lazy : lazys) {
            CHECK(lazy.is_finished());
            results.push_back(lazy.get_result().result());
        }
        return results;
    }

    std::vector<corio::Lazy<int>> lazys;
    int count;
};

struct RemoteAwaiter {
    struct SharedState {
        std::mutex mu;
        std::coroutine_handle<> handle;
        asio::any_io_executor strand;
        std::optional<corio::Result<int>> result;
    };

    bool await_ready() noexcept { return false; }

    template <typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        std::lock_guard<std::mutex> lock(shared_state->mu);
        if (shared_state->result.has_value()) {
            return false;
        }
        shared_state->handle = handle;
        shared_state->strand = handle.promise().strand();
        return true;
    }

    int await_resume() noexcept {
        std::lock_guard<std::mutex> lock(shared_state->mu);
        return shared_state->result->result();
    }

    std::shared_ptr<SharedState> shared_state;
};

struct ChangeExecutorAwaiter {
    bool await_ready() noexcept { return false; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        auto &promise = handle.promise();
        auto old_strand = promise.strand();
        promise.set_executor(executor);
        asio::post(old_strand, [handle] {
            asio::post(handle.promise().strand(),
                       [handle] { handle.resume(); });
        });
    }

    void await_resume() noexcept {}

    asio::any_io_executor executor;
};

} // namespace

TEST_CASE("test lazy") {

    SUBCASE("basic") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };

        auto lazy = f();
        CHECK(!lazy.is_finished());
        CHECK_THROWS(lazy.execute());

        asio::io_context io_context;
        lazy.set_executor(io_context.get_executor());
        CHECK_NOTHROW(lazy.execute());
        CHECK(!lazy.is_finished());

        io_context.run();
        CHECK(lazy.is_finished());
        CHECK(lazy.get_result().result() == 1);
    }

    SUBCASE("resouce manage") {
        auto f = []() -> corio::Lazy<int> { co_return 1; };

        auto tmp = f();
        CHECK(tmp);

        corio::Lazy<int> lazy;
        CHECK(!lazy);

        lazy = std::move(tmp);
        CHECK(lazy);
        CHECK(!tmp);

        auto h = lazy.release();
        CHECK(!lazy);
        CHECK(h != nullptr);

        tmp.reset(h);
        CHECK(tmp);

        tmp.reset();
        CHECK(!tmp);
    }

    SUBCASE("nested function") {
        auto f1 = []() -> corio::Lazy<int> { co_return 1; };
        auto f2 = [&]() -> corio::Lazy<int> {
            auto n = co_await f1();
            co_return n + co_await f1();
        };

        auto lazy = f2();
        CHECK(!lazy.is_finished());
        asio::io_context io_context;
        lazy.set_executor(io_context.get_executor());
        lazy.execute();

        io_context.run();

        CHECK(lazy.is_finished());
        CHECK(lazy.get_result().result() == 2);
    }

    SUBCASE("simple callback") {
        auto f = []() -> corio::Lazy<int> {
            co_await SimpleAwaiter{};
            co_return 1;
        };

        auto lazy = f();

        auto callback = [](corio::Lazy<int> lazy, int &n) -> corio::Lazy<void> {
            int r = co_await lazy;
            n = r;
        };

        int n = 0;
        auto entry = callback(std::move(lazy), n);

        asio::io_context io_context;
        entry.set_executor(io_context.get_executor());
        entry.execute();

        io_context.run();

        CHECK(n == 1);
    }

    SUBCASE("simple gather") {
        auto f = [](int a) -> corio::Lazy<int> {
            co_await SimpleAwaiter{};
            co_return a;
        };

        auto main = [&]() -> corio::Lazy<std::vector<int>> {
            GatherAwaiter awaiter;
            awaiter.lazys.push_back(f(1));
            awaiter.lazys.push_back(f(2));
            awaiter.lazys.push_back(f(3));
            co_return co_await awaiter;
        };

        auto lazy = main();

        asio::io_context io_context;
        lazy.set_executor(io_context.get_executor());
        lazy.execute();

        io_context.run();

        CHECK(lazy.is_finished());
        auto results = lazy.get_result().result();
        CHECK(results.size() == 3);
        CHECK(results[0] == 1);
        CHECK(results[1] == 2);
        CHECK(results[2] == 3);
    }

    SUBCASE("cross executor callback") {
        asio::io_context io1, io2;
        auto io1_guard = asio::make_work_guard(io1);

        auto f = [&]() -> corio::Lazy<int> {
            // heavy task
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            co_return 42;
        };

        auto shared_state = std::make_shared<RemoteAwaiter::SharedState>();
        auto remote =
            [](std::shared_ptr<RemoteAwaiter::SharedState> shared_state,
               corio::Lazy<int> lazy) -> corio::Lazy<void> {
            auto r = co_await lazy;

            std::lock_guard<std::mutex> lock(shared_state->mu);
            shared_state->result = corio::Result<int>::from_result(r);
            if (shared_state->handle) {
                asio::post(
                    shared_state->strand,
                    [handle = shared_state->handle]() { handle.resume(); });
            }
        };

        auto local =
            [&](std::shared_ptr<RemoteAwaiter::SharedState> shared_state)
            -> corio::Lazy<int> {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            auto r = co_await RemoteAwaiter{shared_state};
            io1_guard.reset();
            co_return r;
        };

        auto remote_lazy = remote(shared_state, f());
        remote_lazy.set_executor(io2.get_executor());
        remote_lazy.execute();

        auto local_lazy = local(shared_state);
        local_lazy.set_executor(io1.get_executor());
        local_lazy.execute();

        std::thread t([&io2] { io2.run(); });

        io1.run();

        t.join();

        CHECK(remote_lazy.is_finished());
        CHECK(local_lazy.is_finished());
        CHECK(local_lazy.get_result().result() == 42);
    }

    SUBCASE("change executor") {
        asio::io_context io1;
        asio::thread_pool io2(1);

        bool called = false;
        auto f = [&](bool &called) -> corio::Lazy<void> {
            auto id1 = std::this_thread::get_id(); // In io1
            co_await ChangeExecutorAwaiter{io2.get_executor()};
            auto id2 = std::this_thread::get_id(); // In io2
            CHECK(id1 != id2);
            called = true;
            co_return;
        };

        auto lazy = f(called);
        lazy.set_executor(io1.get_executor());
        lazy.execute();

        io1.run();

        io2.join();

        CHECK(called);
    }
}