#pragma once

#include <asio.hpp>
#include <optional>
#include <variant>

namespace corio::detail {

class SerialRunner {
public:
    SerialRunner() = default;

    SerialRunner(const asio::any_io_executor &serial_executor)
        : serial_executor_(serial_executor) {}

    SerialRunner(const asio::strand<asio::any_io_executor> &strand)
        : serial_executor_(strand) {}

    operator bool() const { return serial_executor_.has_value(); }

    asio::any_io_executor get_executor() const {
        return std::visit(
            [](const auto &e) -> asio::any_io_executor { return e; },
            serial_executor_.value());
    }

    SerialRunner fork_runner() const {
        return SerialRunner(get_inner_executor());
    }

    asio::any_io_executor get_inner_executor() const {
        return std::visit(
            [](const auto &e) -> asio::any_io_executor {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, asio::any_io_executor>) {
                    return e;
                } else {
                    return e.get_inner_executor();
                }
            },
            serial_executor_.value());
    }

private:
    using VariantExecutor = std::variant<asio::any_io_executor,
                                         asio::strand<asio::any_io_executor>>;
    std::optional<VariantExecutor> serial_executor_;
};

} // namespace corio::detail