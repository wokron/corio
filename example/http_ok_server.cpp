#include <asio.hpp>
#include <corio.hpp>

using tcp = asio::ip::tcp;

namespace corio {
using acceptor = corio::use_corio_t::as_default_on_t<tcp::acceptor>;
using socket = corio::use_corio_t::as_default_on_t<tcp::socket>;
} // namespace corio

constexpr char RESPONSE[] = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!
)";

corio::Lazy<void> session(corio::socket socket) {
    co_await asio::async_write(socket, asio::buffer(RESPONSE));
}

corio::Lazy<void> http_server(unsigned short port) {
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
    unsigned short port = std::atoi(argv[1]);

    corio::run(http_server(port));
}