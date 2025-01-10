#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/this_coro.hpp"
#include <asio.hpp>

namespace corio::detail {

class PromiseBase {
public:
    this_coro::detail::ExecutorAwaiter
    await_transform(const this_coro::detail::ExecutorPlaceholder &) {
        return {.executor = executor_};
    }

    this_coro::detail::StrandAwaiter
    await_transform(const this_coro::detail::StrandPlaceholder &) {
        CORIO_ASSERT(strand_.has_value(), "The strand is not set");
        return {.strand = strand_.value()};
    }

    template <detail::awaitable Awaitable>
    Awaitable &&await_transform(Awaitable &&awaitable) {
        return std::forward<Awaitable>(awaitable);
    }

public:
    void set_executor(asio::any_io_executor executor) {
        executor_ = executor;
        strand_ = asio::make_strand(executor_);
    }

    const asio::any_io_executor &executor() const { return executor_; }

    void set_strand(asio::strand<asio::any_io_executor> strand) {
        strand_ = strand;
        executor_ = strand.get_inner_executor();
    }

    const asio::strand<asio::any_io_executor> &strand() const {
        CORIO_ASSERT(strand_.has_value(), "The strand is not set");
        return strand_.value();
    }

private:
    asio::any_io_executor executor_;
    std::optional<asio::strand<asio::any_io_executor>> strand_;
};

} // namespace corio::detail