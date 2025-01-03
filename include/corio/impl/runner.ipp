#pragma once

#include "asio/any_io_executor.hpp"
#include "asio/thread_pool.hpp"
#include "corio/detail/singleton.hpp"
#include "corio/runner.hpp"
#include "corio/task.hpp"
#include <future>
#include <type_traits>

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

template <typename T>
Lazy<void> launch_with_future(Lazy<T> lazy, std::promise<T> promise) {
    if constexpr (std::is_void_v<T>) {
        co_await lazy;
        promise.set_value();
    } else {
        promise.set_value(co_await lazy);
    }
}

} // namespace detail

inline asio::any_io_executor get_default_executor() noexcept {
    detail::DefaultExecutor::init(16);
    return detail::DefaultExecutor::get().get_executor();
}

template <typename T>
inline T block_on(asio::any_io_executor executor, Lazy<T> lazy) {
    std::promise<T> promise;
    std::future<T> future = promise.get_future();
    Task<void> task =
        spawn(executor,
              detail::launch_with_future(std::move(lazy), std::move(promise)));
    return future.get();
}

template <typename T> inline T run(Lazy<T> lazy) {
    return block_on(get_default_executor(), std::move(lazy));
}

} // namespace corio
