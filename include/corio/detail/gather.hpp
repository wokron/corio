#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/try_gather.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <optional>
#include <type_traits>
#include <vector>

namespace corio::detail {

template <awaitable... Awaitables> class TupleGatherAwaiter;

template <awaitable... Awaitables> class TupleGatherSharedState {
public:
    using ReturnType = typename TupleGatherAwaiter<Awaitables...>::ReturnType;

    void init(std::size_t rest_count, std::coroutine_handle<> resume_handle,
              asio::strand<asio::any_io_executor> strand) noexcept {
        rest_count_ = rest_count;
        resume_handle_ = resume_handle;
        strand_ = strand;
    }

    const asio::strand<asio::any_io_executor> &strand() const noexcept {
        return strand_.value();
    }

    bool dec_and_check() noexcept { return --rest_count_ == 0; }

    ReturnType &results() noexcept { return results_; }

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

    void cancel() noexcept { *canceled_ = true; }

    ReturnType unwrap_results() { return std::move(results_); }

private:
    std::size_t rest_count_;

    ReturnType results_;

    std::coroutine_handle<> resume_handle_ = nullptr;
    std::optional<asio::strand<asio::any_io_executor>> strand_;

    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

template <awaitable... Awaitables> class TupleGatherAwaiter {
public:
    using ReturnType =
        std::tuple<corio::Result<awaitable_return_t<Awaitables>>...>;

    explicit TupleGatherAwaiter(Awaitables &&...awaitables) noexcept
        : awaitables_(keep_ref(std::forward<Awaitables>(awaitables))...) {}

public:
    TupleGatherAwaiter(const TupleGatherAwaiter &) = delete;
    TupleGatherAwaiter &operator=(const TupleGatherAwaiter &) = delete;

    TupleGatherAwaiter(TupleGatherAwaiter &&) = default;
    TupleGatherAwaiter &operator=(TupleGatherAwaiter &&) = default;

    ~TupleGatherAwaiter() {}

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) {
        state_.init(sizeof...(Awaitables), handle, handle.promise().strand());
        launch_all_co_await(awaitables_, state_);
    }

    ReturnType await_resume() { return state_.unwrap_results(); }

private:
    using SharedState = TupleGatherSharedState<Awaitables...>;

    template <std::size_t I = 0, typename... Args>
    void launch_all_co_await(std::tuple<Args...> &args, SharedState &state) {
        if constexpr (I < sizeof...(Args)) {
            auto &awaitable = std::get<I>(args);

            // Tuple foreach begin
            corio::Lazy<void> lazy = do_co_await<I>(awaitable, state);
            lazy.set_strand(state.strand());
            lazy.execute();
            lazies_.push_back(std::move(lazy)); // Keep lazy alive
            // Tuple foreach end

            launch_all_co_await<I + 1>(args, state);
        }
    }

    template <std::size_t I, typename Arg>
    corio::Lazy<void> do_co_await(Arg &arg, SharedState &state) {
        using Return = at_position_t<I, awaitable_return_t<Awaitables>...>;
        auto &result = std::get<I>(state.results());
        try {
            if constexpr (std::is_void_v<Return>) {
                co_await arg;
                result = corio::Result<Return>::from_result();
            } else {
                result = corio::Result<Return>::from_result(co_await arg);
            }
        } catch (...) {
            result =
                corio::Result<Return>::from_exception(std::current_exception());
        }

        if (state.dec_and_check()) {
            state.resume();
        }
    }

private:
    using AwaitablesTuple =
        std::tuple<decltype(keep_ref(std::declval<Awaitables>()))...>;

    AwaitablesTuple awaitables_;
    SharedState state_;
    std::vector<corio::Lazy<void>> lazies_;
};

template <awaitable_iterable Iterable> class IterGatherAwaiter;

template <awaitable_iterable Iterable> class IterGatherSharedState {
public:
    using ReturnType = typename IterGatherAwaiter<Iterable>::ReturnType;

    void init(std::size_t rest_count, std::coroutine_handle<> resume_handle,
              asio::strand<asio::any_io_executor> strand) noexcept {
        rest_count_ = rest_count;
        resume_handle_ = resume_handle;
        strand_ = strand;
        results_.resize(rest_count);
    }

    const asio::strand<asio::any_io_executor> &strand() const noexcept {
        return strand_.value();
    }

    bool dec_and_check() noexcept { return --rest_count_ == 0; }

    ReturnType &results() noexcept { return results_; }

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

    void cancel() noexcept { *canceled_ = true; }

    ReturnType unwrap_results() { return std::move(results_); }

private:
    std::size_t rest_count_;

    ReturnType results_;

    std::coroutine_handle<> resume_handle_ = nullptr;
    std::optional<asio::strand<asio::any_io_executor>> strand_;

    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

template <awaitable_iterable Iterable> class IterGatherAwaiter {
public:
    using Awaitable = std::iter_value_t<Iterable>;
    using AwaitableReturn = awaitable_return_t<Awaitable>;
    using ReturnType =
        std::vector<corio::Result<void_to_monostate_t<AwaitableReturn>>>;

    explicit IterGatherAwaiter(Iterable &&iterable) noexcept
        : iterable_(std::forward<Iterable>(iterable)) {}

public:
    IterGatherAwaiter(const IterGatherAwaiter &) = delete;
    IterGatherAwaiter &operator=(const IterGatherAwaiter &) = delete;

    IterGatherAwaiter(IterGatherAwaiter &&) = default;
    IterGatherAwaiter &operator=(IterGatherAwaiter &&) = default;

    ~IterGatherAwaiter() { state_.cancel(); }

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) {
        std::size_t total = std::size(iterable_);
        state_.init(total, handle, handle.promise().strand());
        launch_all_co_await();
    }

    ReturnType await_resume() { return state_.unwrap_results(); }

private:
    using SharedState = IterGatherSharedState<Iterable>;

    void launch_all_co_await() {
        std::size_t no = 0;
        for (auto &awaitable : iterable_) {
            corio::Lazy<void> lazy = do_co_await(no, awaitable);
            lazy.set_strand(state_.strand());
            lazy.execute();
            lazies_.push_back(std::move(lazy)); // Keep lazy alive
            no++;
        }
    }

    corio::Lazy<void> do_co_await(std::size_t no, Awaitable &awaitable) {
        auto &result = state_.results()[no];
        try {
            if constexpr (std::is_void_v<AwaitableReturn>) {
                co_await awaitable;
                result = corio::Result<AwaitableReturn>::from_result();
            } else {
                result = corio::Result<AwaitableReturn>::from_result(
                    co_await awaitable);
            }
        } catch (...) {
            result = corio::Result<AwaitableReturn>::from_exception(
                std::current_exception());
        }

        if (state_.dec_and_check()) {
            state_.resume();
        }
    }

private:
    Iterable iterable_;
    SharedState state_;
    std::vector<corio::Lazy<void>> lazies_;
};

} // namespace corio::detail