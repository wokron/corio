#include <asio.hpp>
#include <corio.hpp>
#include <iostream>
#include <string>

using tcp = asio::ip::tcp;

namespace corio {
using acceptor = corio::use_corio_t::as_default_on_t<tcp::acceptor>;
using socket = corio::use_corio_t::as_default_on_t<tcp::socket>;
} // namespace corio

corio::Lazy<void> session(corio::socket socket) {
    auto ex = co_await corio::this_coro::executor;
    std::byte data[1024];
    while (true) {
        auto n = co_await socket.async_read_some(asio::buffer(data));
        co_await asio::async_write(socket, asio::buffer(data, n));
    }
}

corio::Lazy<void> echo_server(unsigned short port) {
    auto ex = co_await corio::this_coro::executor;
    corio::acceptor acceptor(ex, tcp::endpoint(tcp::v4(), port));

    while (true) {
        corio::socket socket(ex);
        co_await acceptor.async_accept(socket);
        co_await corio::spawn_background(session(std::move(socket)));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    unsigned short port = std::stoi(argv[1]);
    corio::run(echo_server(port));

    return 0;
}