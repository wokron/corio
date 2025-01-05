#pragma once

#include <asio.hpp>
#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include "corio/result.hpp"

namespace corio::detail {

template <typename AwaitablesType, typename Collector>
class BasicCollectAwaiter {
public:
    explicit BasicCollectAwaiter(AwaitablesType &&awaitables) noexcept
        : awaitables_(std::forward<AwaitablesType>(awaitables)) {}

public:
    BasicCollectAwaiter(const BasicCollectAwaiter &) = delete;
    BasicCollectAwaiter &operator=(const BasicCollectAwaiter &) = delete;

    BasicCollectAwaiter(BasicCollectAwaiter &&) = default;
    BasicCollectAwaiter &operator=(BasicCollectAwaiter &&) = default;

    ~BasicCollectAwaiter() { collector_.cancel(); }

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        collector_.launch_all_await(awaitables_, handle);
    }

    auto await_resume() { return collector_.unwrap_results(); }

private:
    AwaitablesType awaitables_;
    Collector collector_;
};

class CollectorBase {
public:
    void cancel() { *canceled_ = true; }

    void resume() {
        if (resume_handle_ != nullptr) {
            auto do_resume = [h = resume_handle_, c = canceled_] {
                bool is_canceled = *c;
                if (!is_canceled) {
                    h.resume();
                }
            };
            asio::post(strand_.value(), do_resume);
            resume_handle_ = nullptr;
        }
    }

protected:
    template <typename Promise>
    void register_handle_(std::coroutine_handle<Promise> h) {
        resume_handle_ = h;
        strand_ = h.promise().strand();
    }

    auto get_strand_() const noexcept { return strand_.value(); }

private:
    std::coroutine_handle<> resume_handle_ = nullptr;
    std::optional<asio::strand<asio::any_io_executor>> strand_;

    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

template <awaitable_iterable Iterable, typename CollectHandler>
class BasicIterCollector : public CollectorBase {
public:
    template <typename Promise>
    void launch_all_await(Iterable &iterable,
                          std::coroutine_handle<Promise> handle) {
        register_handle_(handle);
        handler_.init(iterable);

        std::size_t no = 0;
        for (auto &awaitable : iterable) {
            corio::Lazy<void> lazy = handler_.do_co_await(*this, no, awaitable);
            lazy.set_strand(get_strand_());
            lazy.execute();
            lazies_.push_back(std::move(lazy)); // Keep lazy alive
            no++;
        }
    }

    auto unwrap_results() { return handler_.unwrap_results(); }

private:
    CollectHandler handler_;

    std::vector<corio::Lazy<void>> lazies_;
};

template <typename Tuple, typename CollectHandler>
class BasicTupleCollector : public CollectorBase {
public:
    template <typename Promise>
    void launch_all_await(Tuple &tuple, std::coroutine_handle<Promise> handle) {
        register_handle_(handle);
        handler_.init(tuple);

        launch_all_await_impl_(tuple);
    }

    auto unwrap_results() { return handler_.unwrap_results(); }

private:
    template <std::size_t I = 0> void launch_all_await_impl_(Tuple &tuple) {
        if constexpr (I < std::tuple_size_v<Tuple>) {
            auto &awaitable = std::get<I>(tuple);

            corio::Lazy<void> lazy =
                handler_.template do_co_await<I>(*this, awaitable);
            lazy.set_strand(get_strand_());
            lazy.execute();
            lazies_.push_back(std::move(lazy)); // Keep lazy alive

            launch_all_await_impl_<I + 1>(tuple);
        }
    }

private:
    CollectHandler handler_;

    std::vector<corio::Lazy<void>> lazies_;
};

} // namespace corio::detail