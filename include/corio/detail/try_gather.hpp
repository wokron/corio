#pragma once

#include "asio.hpp"
#include "asio/any_io_executor.hpp"
#include "asio/strand.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <type_traits>

namespace corio::detail {

template <typename T> auto keep_ref(T &&value) {
    if constexpr (std::is_lvalue_reference_v<T>) {
        return std::ref(value);
    } else {
        return value;
    }
}

template <awaitable... Awaitables> class TupleTryGatherAwaiter;

template <awaitable... Awaitables> class TupleTryGatherSharedState {
public:
    using ReturnType =
        typename TupleTryGatherAwaiter<Awaitables...>::ReturnType;

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

    std::exception_ptr &first_exception() noexcept { return first_exception_; }

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

    ReturnType unwrap_results() {
        if (first_exception_ != nullptr) {
            std::rethrow_exception(first_exception_);
        }
        return std::move(results_);
    }

private:
    std::size_t rest_count_;

    ReturnType results_;
    std::exception_ptr first_exception_ = nullptr;

    std::coroutine_handle<> resume_handle_ = nullptr;
    std::optional<asio::strand<asio::any_io_executor>> strand_;

    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

template <awaitable... Awaitables> class TupleTryGatherAwaiter {
public:
    using ReturnType =
        std::tuple<void_to_monostate_t<awaitable_return_t<Awaitables>>...>;

    explicit TupleTryGatherAwaiter(Awaitables &&...awaitables) noexcept
        : awaitables_(keep_ref(std::forward<Awaitables>(awaitables))...) {}

public:
    TupleTryGatherAwaiter(const TupleTryGatherAwaiter &) = delete;
    TupleTryGatherAwaiter &operator=(const TupleTryGatherAwaiter &) = delete;

    TupleTryGatherAwaiter(TupleTryGatherAwaiter &&) = default;
    TupleTryGatherAwaiter &operator=(TupleTryGatherAwaiter &&) = default;

    ~TupleTryGatherAwaiter() { state_.cancel(); }

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        state_.init(sizeof...(Awaitables), handle, handle.promise().strand());
        launch_all_co_await(awaitables_, state_);
    }

    ReturnType await_resume() { return state_.unwrap_results(); }

private:
    using SharedState = TupleTryGatherSharedState<Awaitables...>;

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
        try {
            if constexpr (std::is_same_v<std::tuple_element_t<I, ReturnType>,
                                         std::monostate>) {
                co_await arg;
            } else {
                std::get<I>(state.results()) = co_await arg;
            }
        } catch (...) {
            if (state.first_exception() == nullptr) {
                state.first_exception() = std::current_exception();
                state.resume();
            }
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

template <awaitable_iterable Iterable> class IterTryGatherAwaiter;

template <awaitable_iterable Iterable> class IterTryGatherSharedState {
public:
    using ReturnType = typename IterTryGatherAwaiter<Iterable>::ReturnType;

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

    std::exception_ptr &first_exception() noexcept { return first_exception_; }

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

    ReturnType unwrap_results() {
        if (first_exception_ != nullptr) {
            std::rethrow_exception(first_exception_);
        }
        return std::move(results_);
    }

private:
    std::size_t rest_count_;

    ReturnType results_;
    std::exception_ptr first_exception_ = nullptr;

    std::coroutine_handle<> resume_handle_ = nullptr;
    std::optional<asio::strand<asio::any_io_executor>> strand_;

    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

template <awaitable_iterable Iterable> class IterTryGatherAwaiter {
public:
    using Awaitable = std::iter_value_t<Iterable>;
    using AwaitableReturn = awaitable_return_t<Awaitable>;
    using ReturnType = std::vector<void_to_monostate_t<AwaitableReturn>>;

    explicit IterTryGatherAwaiter(Iterable &&iterable) noexcept
        : iterable_(std::forward<Iterable>(iterable)) {}

public:
    IterTryGatherAwaiter(const IterTryGatherAwaiter &) = delete;
    IterTryGatherAwaiter &operator=(const IterTryGatherAwaiter &) = delete;

    IterTryGatherAwaiter(IterTryGatherAwaiter &&) = default;
    IterTryGatherAwaiter &operator=(IterTryGatherAwaiter &&) = default;

    ~IterTryGatherAwaiter() { state_.cancel(); }

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        std::size_t total = std::distance(iterable_.begin(), iterable_.end());
        state_.init(total, handle, handle.promise().strand());
        launch_all_co_await();
    }

    ReturnType await_resume() { return state_.unwrap_results(); }

private:
    using SharedState = IterTryGatherSharedState<Iterable>;

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
        try {
            if constexpr (std::is_same_v<void_to_monostate_t<AwaitableReturn>,
                                         std::monostate>) {
                co_await awaitable;
            } else {
                state_.results().at(no) = co_await awaitable;
            }
        } catch (...) {
            if (state_.first_exception() == nullptr) {
                state_.first_exception() = std::current_exception();
                state_.resume();
            }
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