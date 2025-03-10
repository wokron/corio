#pragma once

#include "corio/detail/collective.hpp"

namespace corio::detail {

template <awaitable_iterable Iterable> class IterGatherCollectHandler {
public:
    using Awaitable = std::iter_value_t<Iterable>;
    using AwaitableReturn = awaitable_return_t<Awaitable>;
    using ReturnType = std::vector<corio::Result<AwaitableReturn>>;

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
                result = corio::Result<AwaitableReturn>::from_result();
            } else {
                result = corio::Result<AwaitableReturn>::from_result(
                    co_await awaitable);
            }
        } catch (...) {
            result = corio::Result<AwaitableReturn>::from_exception(
                std::current_exception());
        }

        rest_count_--;
        if (rest_count_ == 0) {
            collector.resume();
        }
    }

    ReturnType collect_results() { return std::move(results_); }

private:
    std::size_t rest_count_;
    ReturnType results_;
};

template <typename T> struct tuple_gather_result {
    using type = corio::Result<awaitable_return_t<T>>;
};

template <typename Tuple> class TupleGatherCollectHandler {
public:
    using ReturnType = transform_tuple_t<Tuple, tuple_gather_result>;

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
                result = corio::Result<T>::from_result();
            } else {
                result = corio::Result<T>::from_result(co_await awaitable);
            }
        } catch (...) {
            result = corio::Result<T>::from_exception(std::current_exception());
        }

        rest_count_--;
        if (rest_count_ == 0) {
            collector.resume();
        }
    }

    ReturnType collect_results() { return std::move(results_); }

private:
    std::size_t rest_count_;
    ReturnType results_;
};

template <awaitable_iterable Iterable>
using IterGatherCollector =
    BasicIterCollector<Iterable, IterGatherCollectHandler<Iterable>>;

template <awaitable_iterable Iterable>
using IterGatherAwaiter =
    BasicCollectAwaiter<Iterable, IterGatherCollector<Iterable>>;

template <typename Tuple>
using TupleGatherCollector =
    BasicTupleCollector<Tuple, TupleGatherCollectHandler<Tuple>>;

template <typename Tuple>
using TupleGatherAwaiter =
    BasicCollectAwaiter<Tuple, TupleGatherCollector<Tuple>>;

} // namespace corio::detail