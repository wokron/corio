#include "corio/run.hpp"
#include "corio/task.hpp"
#include <asio.hpp>
#include <corio/lazy.hpp>
#include <corio/operation.hpp>
#include <corio/this_coro.hpp>
#include <doctest/doctest.h>
#include <thread>

using namespace std::chrono_literals;

namespace {

[[clang::optnone]] std::thread::id get_tid() {
    return std::this_thread::get_id();
}

} // namespace

TEST_CASE("test operation") {

    SUBCASE("timer operation basic") {
        auto f = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            asio::steady_timer timer(ex, 100us);

            auto start = std::chrono::steady_clock::now();
            co_await timer.async_wait(corio::use_corio);
            auto end = std::chrono::steady_clock::now();

            CHECK((end - start) >= 100us);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());
    }

    SUBCASE("timer operation with cancel") {
        auto f = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            asio::steady_timer timer(ex, 100us);

            bool called = false;

            auto g = [&]() -> corio::Lazy<void> {
                called = true;
                co_await timer.async_wait(corio::use_corio);
                CHECK(false);
            };

            auto task = co_await corio::spawn(g());

            co_await corio::this_coro::sleep_for(20us);

            task.abort();

            co_await corio::this_coro::sleep_for(100us);

            CHECK(called);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());
    }

    SUBCASE("timer operation with different executor") {
        asio::thread_pool pool1(1), pool2(1);

        auto f = [&]() -> corio::Lazy<void> {
            auto id1 = get_tid();
            co_await asio::post(pool2.get_executor(), corio::use_corio);
            auto id2 = get_tid();

            CHECK(id1 != id2);
        };

        corio::block_on(pool1.get_executor(), f());
    }

    SUBCASE("test as default on") {
        auto f = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            auto timer =
                corio::use_corio.as_default_on(asio::steady_timer(ex, 100us));

            auto start = std::chrono::steady_clock::now();
            co_await timer.async_wait();
            auto end = std::chrono::steady_clock::now();

            CHECK((end - start) >= 100us);
        };

        auto g = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;
            using steady_timer =
                corio::use_corio_t::as_default_on_t<asio::steady_timer>;
            auto timer = steady_timer(ex, 100us);

            auto start = std::chrono::steady_clock::now();
            co_await timer.async_wait();
            auto end = std::chrono::steady_clock::now();

            CHECK((end - start) >= 100us);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());

        corio::block_on(pool.get_executor(), g());
    }

    SUBCASE("test default executor") {
        auto f = []() -> corio::Lazy<void> {
            auto id1 = get_tid();
            co_await asio::post(corio::use_corio);
            auto id2 = get_tid();
            CHECK(id1 == id2);
        };

        asio::thread_pool pool(1);

        corio::block_on(pool.get_executor(), f());
    }
}

TEST_CASE("test tcp operations") {

    SUBCASE("tcp echo server") {
        auto f = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;

            asio::ip::tcp::acceptor acceptor(
                ex, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
            auto port = acceptor.local_endpoint().port();

            auto g = [&]() -> corio::Lazy<void> {
                asio::ip::tcp::socket socket(ex);
                co_await acceptor.async_accept(socket, corio::use_corio);

                std::array<char, 1024> buffer;
                auto n = co_await socket.async_read_some(asio::buffer(buffer),
                                                         corio::use_corio);
                co_await asio::async_write(socket, asio::buffer(buffer, n),
                                           corio::use_corio);
            };

            co_await corio::spawn_background(g());

            asio::ip::tcp::socket socket(ex);
            socket.connect(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));

            std::array<char, 5> buffer = {'a', 'b', 'c', 'd', 'e'};
            co_await asio::async_write(socket, asio::buffer(buffer),
                                       corio::use_corio);

            std::array<char, 1024> recv_buffer;
            auto n = co_await socket.async_read_some(asio::buffer(recv_buffer),
                                                     corio::use_corio);

            CHECK(n == 5);
            CHECK(std::equal(buffer.begin(), buffer.begin() + n,
                             recv_buffer.begin()));
        };

        asio::thread_pool pool(3);

        corio::block_on(asio::make_strand(pool.get_executor()), f());
    }

    SUBCASE("tcp echo server with cancel") {
        auto f = []() -> corio::Lazy<void> {
            auto ex = co_await corio::this_coro::executor;

            asio::ip::tcp::acceptor acceptor(
                ex, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
            auto port = acceptor.local_endpoint().port();

            auto g = [&]() -> corio::Lazy<void> {
                asio::ip::tcp::socket socket(ex);
                while (true) {
                    co_await acceptor.async_accept(socket, corio::use_corio);

                    std::array<char, 1024> buffer;
                    auto n = co_await socket.async_read_some(
                        asio::buffer(buffer), corio::use_corio);
                    co_await asio::async_write(socket, asio::buffer(buffer, n),
                                               corio::use_corio);
                }
            };

            auto task = co_await corio::spawn(g());

            asio::ip::tcp::socket socket(ex);
            socket.connect(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));

            std::array<char, 5> buffer = {'a', 'b', 'c', 'd', 'e'};
            co_await asio::async_write(socket, asio::buffer(buffer),
                                       corio::use_corio);

            std::array<char, 1024> recv_buffer;
            auto n = co_await socket.async_read_some(asio::buffer(recv_buffer),
                                                     corio::use_corio);

            CHECK(n == 5);
            CHECK(std::equal(buffer.begin(), buffer.begin() + n,
                             recv_buffer.begin()));

            co_await corio::this_coro::sleep_for(100us);
            task.abort();
            co_await corio::this_coro::sleep_for(100us);
        };

        asio::thread_pool pool(3);

        corio::block_on(asio::make_strand(pool.get_executor()), f());
    }
}