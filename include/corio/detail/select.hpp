#pragma once

#include "corio/detail/collective.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include <utility>
#include <variant>

namespace corio::detail {

template <awaitable_iterable Iterable> class IterSelectCollectHandler {
public:
    using Awaitable = std::iter_value_t<Iterable>;
    using AwaitableReturn = awaitable_return_t<Awaitable>;
    using ReturnType =
        std::pair<std::size_t, void_to_monostate_t<AwaitableReturn>>;

    void init(Iterable &iterable) {
        // DO NOTHING
    }

    corio::Lazy<void> do_co_await(CollectorBase &collector, std::size_t no,
                                  Awaitable &awaitable) {
        using Return = void_to_monostate_t<AwaitableReturn>;
        try {
            if constexpr (std::is_void_v<AwaitableReturn>) {
                co_await awaitable;
                if (no_ == -1) {
                    no_ = no;
                    result_ =
                        corio::Result<Return>::from_result(std::monostate{});
                    collector.resume();
                }
            } else {
                auto &&r = co_await awaitable;
                if (no_ == -1) {
                    no_ = no;
                    result_ = corio::Result<Return>::from_result(
                        std::forward<decltype(r)>(r));
                    collector.resume();
                }
            }
        } catch (...) {
            if (no_ == -1) {
                no_ = no;
                result_ = corio::Result<Return>::from_exception(
                    std::current_exception());
                collector.resume();
            }
        }
    }

    ReturnType collect_results() { return {no_, std::move(result_.result())}; }

private:
    std::size_t no_ = -1;
    corio::Result<void_to_monostate_t<AwaitableReturn>> result_;
};

template <typename T> struct tuple_select_result {
    using type = void_to_monostate_t<awaitable_return_t<T>>;
};

template <typename Tuple> class TupleSelectCollectHandler {
public:
    using ReturnType =
        apply_each_t<std::variant,
                     transform_tuple_t<Tuple, tuple_select_result>>;

    void init(Tuple &tuple) {
        // DO NOTHING
    }

    template <std::size_t I, typename Awaitable>
    corio::Lazy<void> do_co_await(CollectorBase &collector,
                                  Awaitable &awaitable) {
        using T = awaitable_return_t<Awaitable>;
        try {
            if constexpr (std::is_void_v<T>) {
                co_await awaitable;
                if (!result_.has_value()) {
                    result_ = corio::Result<ReturnType>::from_result(
                        ReturnType{std::in_place_index<I>, std::monostate{}});
                    collector.resume();
                }
            } else {
                auto &&r = co_await awaitable;
                if (!result_.has_value()) {
                    result_ = corio::Result<ReturnType>::from_result(ReturnType{
                        std::in_place_index<I>, std::forward<decltype(r)>(r)});
                    collector.resume();
                }
            }
        } catch (...) {
            if (!result_.has_value()) {
                result_ = corio::Result<ReturnType>::from_exception(
                    std::current_exception());
                collector.resume();
            }
        }
    }

    ReturnType collect_results() { return std::move(result_.value().result()); }

private:
    std::optional<corio::Result<ReturnType>> result_;
};

template <awaitable_iterable Iterable>
using IterSelectCollector =
    BasicIterCollector<Iterable, IterSelectCollectHandler<Iterable>>;

template <awaitable_iterable Iterable>
using IterSelectAwaiter =
    BasicCollectAwaiter<Iterable, IterSelectCollector<Iterable>>;

template <typename Tuple>
using TupleSelectCollector =
    BasicTupleCollector<Tuple, TupleSelectCollectHandler<Tuple>>;

template <typename Tuple>
using TupleSelectAwaiter =
    BasicCollectAwaiter<Tuple, TupleSelectCollector<Tuple>>;

} // namespace corio::detail