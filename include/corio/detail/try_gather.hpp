#pragma once

#include "corio/detail/collective.hpp"
#include "corio/detail/concepts.hpp"

namespace corio::detail {

template <awaitable_iterable Iterable> class IterTryGatherCollectHandler {
public:
    using Awaitable = std::iter_value_t<Iterable>;
    using AwaitableReturn = awaitable_return_t<Awaitable>;
    using ReturnType = std::vector<void_to_monostate_t<AwaitableReturn>>;

    void init(Iterable &iterable) {
        std::size_t total = std::size(iterable);
        rest_count_ = total;
        results_.resize(total);
    }

    corio::Lazy<void> do_co_await(CollectorBase &collector, std::size_t no,
                                  Awaitable &awaitable) {
        auto &result = results_[no];
        try {
            if constexpr (std::is_void_v<AwaitableReturn>) {
                co_await awaitable;
            } else {
                result = co_await awaitable;
            }
        } catch (...) {
            if (first_exception_ == nullptr) {
                first_exception_ = std::current_exception();
                collector.resume();
            }
        }

        rest_count_--;
        if (rest_count_ == 0) {
            collector.resume();
        }
    }

    ReturnType collect_results() {
        if (first_exception_ != nullptr) {
            std::rethrow_exception(first_exception_);
        }
        return std::move(results_);
    }

private:
    std::size_t rest_count_;
    ReturnType results_;
    std::exception_ptr first_exception_;
};

template <typename T> struct tuple_try_gather_result {
    using type = void_to_monostate_t<awaitable_return_t<T>>;
};

template <typename Tuple> class TupleTryGatherCollectHandler {
public:
    using ReturnType = transform_tuple_t<Tuple, tuple_try_gather_result>;

    void init(Tuple &) {
        std::size_t total = std::tuple_size_v<Tuple>;
        rest_count_ = total;
    }

    template <std::size_t I, typename Awaitable>
    corio::Lazy<void> do_co_await(CollectorBase &collector,
                                  Awaitable &awaitable) {
        using T = awaitable_return_t<Awaitable>;
        auto &result = std::get<I>(results_);
        try {
            if constexpr (std::is_void_v<T>) {
                co_await awaitable;
            } else {
                result = co_await awaitable;
            }
        } catch (...) {
            if (first_exception_ == nullptr) {
                first_exception_ = std::current_exception();
                collector.resume();
            }
        }

        rest_count_--;
        if (rest_count_ == 0) {
            collector.resume();
        }
    }

    ReturnType collect_results() {
        if (first_exception_ != nullptr) {
            std::rethrow_exception(first_exception_);
        }
        return std::move(results_);
    }

private:
    std::size_t rest_count_;
    ReturnType results_;
    std::exception_ptr first_exception_;
};

template <awaitable_iterable Iterable>
using IterTryGatherCollector =
    BasicIterCollector<Iterable, IterTryGatherCollectHandler<Iterable>>;

template <awaitable_iterable Iterable>
using IterTryGatherAwaiter =
    BasicCollectAwaiter<Iterable, IterTryGatherCollector<Iterable>>;

template <typename Tuple>
using TupleTryGatherCollector =
    BasicTupleCollector<Tuple, TupleTryGatherCollectHandler<Tuple>>;

template <typename Tuple>
using TupleTryGatherAwaiter =
    BasicCollectAwaiter<Tuple, TupleTryGatherCollector<Tuple>>;

} // namespace corio::detail