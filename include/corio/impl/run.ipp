#pragma once

#include "corio/lazy.hpp"
#include "corio/run.hpp"
#include "corio/task.hpp"
#include <asio.hpp>

namespace corio {

namespace detail {

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
inline detail::awaitable_return_t<Awaitable> run(Awaitable aw,
                                                 bool multi_thread) {
    // Since asio already optimizes for single-threaded use cases, we can just
    // use the default thread pool.
    std::size_t thread_count =
        multi_thread ? std::thread::hardware_concurrency() : 1;
    asio::thread_pool pool(thread_count);
    auto executor = pool.get_executor();
    if (multi_thread) {
        auto serial_executor = asio::make_strand(executor);
        return block_on(serial_executor, std::move(aw));
    } else {
        return block_on(executor, std::move(aw));
    }
}

} // namespace corio