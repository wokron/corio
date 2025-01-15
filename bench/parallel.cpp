#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/channel.hpp>
#include <corio.hpp>
#include <iostream>
#include <marker.hpp>

constexpr std::size_t n = 3'000'000;

corio::Lazy<void> corio_test() {
    using corio::awaitable_operators::operator&&;

    auto ex = co_await corio::this_coro::executor;

    asio::experimental::channel<void(asio::error_code)> chan(ex, 0);
    for (std::size_t i = 0; i < n; i++) {
        co_await (chan.async_send(asio::error_code{}, corio::use_corio) &&
                  chan.async_receive(corio::use_corio));
    }
}

void launch_corio_test() {
    asio::io_context ctx{ASIO_CONCURRENCY_HINT_1};
    corio::spawn_background(ctx.get_executor(), corio_test());
    ctx.run();
}

asio::awaitable<void> asio_test() {
    using asio::experimental::awaitable_operators::operator&&;

    auto ex = co_await asio::this_coro::executor;

    asio::experimental::channel<void(asio::error_code)> chan(ex, 0);
    for (std::size_t i = 0; i < n; i++) {
        co_await (chan.async_send(asio::error_code{}, asio::use_awaitable) &&
                  chan.async_receive(asio::use_awaitable));
    }
}

void launch_asio_test() {
    asio::io_context ctx{ASIO_CONCURRENCY_HINT_1};
    asio::co_spawn(ctx, asio_test(), asio::detached);
    ctx.run();
}

int main() {
    for (std::size_t i = 0; i < 6; i++) {
        auto dur = marker::measured(launch_asio_test)();
        std::cerr << "asio: " << dur << std::endl;
    }

    for (std::size_t i = 0; i < 6; i++) {
        auto dur = marker::measured(launch_corio_test)();
        std::cerr << "corio: " << dur << std::endl;
    }

    return 0;
}