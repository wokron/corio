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

template <detail::awaitable Awaitable,
          typename Return = detail::awaitable_return_t<Awaitable>>
Lazy<void> launch_with_future(Awaitable &&awaitable,
                              std::promise<Return> promise) {
    try {
        if constexpr (std::is_void_v<Return>) {
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
    detail::DefaultExecutor::init(16); // TODO: Need to pass a lambda to init
    return detail::DefaultExecutor::get().get_executor();
}

template <detail::awaitable Awaitable, typename Return>
inline Return block_on(asio::any_io_executor executor, Awaitable &&awaitable) {
    std::promise<Return> promise;
    std::future<Return> future = promise.get_future();
    Task<void> task = spawn(
        executor, detail::launch_with_future(std::forward<Awaitable>(awaitable),
                                             std::move(promise)));
    return future.get();
}

template <detail::awaitable Awaitable, typename Return>
inline Return run(Awaitable &&awaitable) {
    return block_on(get_default_executor(), std::forward<Awaitable>(awaitable));
}

} // namespace corio
