#pragma once

#include "asio/any_io_executor.hpp"
#include <asio.hpp>
#include <optional>
#include <variant>

namespace corio::detail {

class SerialRunner {
public:
    SerialRunner() = default;

    SerialRunner(const asio::any_io_executor &serial_executor)
        : serial_executor_(serial_executor) {}

    template <typename Executor>
    SerialRunner(const asio::strand<Executor> &strand) {
        serial_executor_ = StrandExecutor{strand, strand.get_inner_executor()};
    }

    operator bool() const { return serial_executor_.has_value(); }

    asio::any_io_executor get_executor() const {
        return std::visit(
            [](const auto &e) -> asio::any_io_executor {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, asio::any_io_executor>) {
                    return e;
                } else {
                    return e.executor;
                }
            },
            serial_executor_.value());
    }

    SerialRunner fork_runner() const {
        auto inner_ex = get_inner_executor();
        bool is_serial = inner_ex == get_executor();
        if (is_serial) {
            return SerialRunner(inner_ex);
        } else {
            return SerialRunner(asio::make_strand(inner_ex));
        }
    }

    asio::any_io_executor get_inner_executor() const {
        return std::visit(
            [](const auto &e) -> asio::any_io_executor {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, asio::any_io_executor>) {
                    return e;
                } else {
                    return e.inner_executor;
                }
            },
            serial_executor_.value());
    }

private:
    struct StrandExecutor {
        asio::any_io_executor executor;
        asio::any_io_executor inner_executor;
    };

    using VariantExecutor = std::variant<asio::any_io_executor, StrandExecutor>;
    std::optional<VariantExecutor> serial_executor_;
};

} // namespace corio::detail