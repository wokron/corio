#include <asio.hpp>
#include <corio.hpp>

using tcp = asio::ip::tcp;
using corio::awaitable_operators::operator||;
using namespace std::chrono_literals;

namespace corio {
using acceptor = corio::use_corio_t::as_default_on_t<tcp::acceptor>;
using socket = corio::use_corio_t::as_default_on_t<tcp::socket>;
} // namespace corio

corio::Lazy<void> handle_one(unsigned short port) {
    auto ex = co_await corio::this_coro::executor;
    corio::acceptor acceptor(ex, tcp::endpoint(tcp::v4(), port));

    corio::socket socket(ex);
    co_await acceptor.async_accept(socket);

    co_await asio::async_write(socket, asio::buffer("What's your name?\n"));

    std::string name;
    auto r = co_await (
        corio::this_coro::sleep_for(3s) ||
        asio::async_read_until(socket, asio::dynamic_buffer(name), '\n'));
    if (r.index() == 0) {
        co_await asio::async_write(socket, asio::buffer("Goodbye\n"));
    } else {
        co_await asio::async_write(socket, asio::buffer("Hello, " + name));
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    unsigned short port = std::stoi(argv[1]);

    corio::run(handle_one(port));

    return 0;
}