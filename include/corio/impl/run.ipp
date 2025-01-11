#pragma once

#include "asio/strand.hpp"
#include "corio/detail/singleton.hpp"
#include "corio/lazy.hpp"
#include "corio/run.hpp"
#include "corio/task.hpp"

namespace corio {

namespace detail {

class DefaultExecutor : public LazySingleton<DefaultExecutor> {
public:
    DefaultExecutor(int threads) : pool_(threads) {}

    asio::any_io_executor get_executor() noexcept {
        return pool_.get_executor();
    }

private:
    asio::thread_pool pool_;
};

template <awaitable Awaitable>
Lazy<void>
launch_with_future(Awaitable awaitable,
                   std::promise<awaitable_return_t<Awaitable>> promise) {
    try {
        if constexpr (std::is_void_v<awaitable_return_t<Awaitable>>) {
            co_await awaitable;
            promise.set_value();
        } else {
            promise.set_value(co_await awaitable);
        }
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

} // namespace detail

inline asio::any_io_executor get_default_executor() noexcept {
    detail::DefaultExecutor::init([] {
        const auto processor_count = std::thread::hardware_concurrency();
        return std::make_unique<detail::DefaultExecutor>(processor_count);
    });
    return detail::DefaultExecutor::get().get_executor();
}

template <typename Executor, detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> block_on(const Executor &executor,
                                                      Awaitable aw) {
    using T = detail::awaitable_return_t<Awaitable>;
    std::promise<T> promise;
    std::future<T> future = promise.get_future();
    Task<void> task =
        spawn(executor,
              detail::launch_with_future(std::move(aw), std::move(promise)));
    return future.get();
}

template <detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> run(Awaitable aw) {
    auto executor = get_default_executor();
    auto serial_executor = asio::make_strand(executor);
    return block_on(serial_executor, std::move(aw));
}

} // namespace corio