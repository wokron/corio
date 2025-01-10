#pragma once

#include "corio/detail/completion_handler.hpp"
#include "corio/detail/type_traits.hpp"
#include <asio.hpp>
#include <memory>
#include <utility>

namespace corio::detail {

struct use_corio_t {};

template <typename Initiation, typename PackedInitArgs, typename... Args>
class Operation {
public:
    using ResultType = typename CompletionHandler<Args...>::ResultType;

    explicit Operation(Initiation initiation, PackedInitArgs init_args)
        : initiation_(std::move(initiation)), init_args_(std::move(init_args)),
          signal_(std::make_unique<asio::cancellation_signal>()) {}

public:
    Operation(const Operation &) = delete;
    Operation &operator=(const Operation &) = delete;

    Operation(Operation &&) = default;
    Operation &operator=(Operation &&) = default;

    ~Operation() {
        if (signal_ && !resumed_) {
            signal_->emit(asio::cancellation_type::all);
            *cancelled_ = true;
        }
    }

public:
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) {
        auto completion_handler =
            CompletionHandler<Args...>(handle, signal_->slot(), result_);
        cancelled_ = completion_handler.get_canceled();

        initiate_(std::move(completion_handler));
    }

    result_value_t<ResultType> await_resume() {
        resumed_ = true;
        if constexpr (std::is_void_v<result_value_t<ResultType>>) {
            result_.result();
            return;
        } else {
            return std::move(result_.result());
        }
    }

private:
    void initiate_(CompletionHandler<Args...> handler) {
        std::apply(
            [&](auto &&...args) {
                std::move(initiation_)(std::move(handler),
                                       std::forward<decltype(args)>(args)...);
            },
            std::move(init_args_));
    }

private:
    Initiation initiation_;
    PackedInitArgs init_args_;

    std::unique_ptr<asio::cancellation_signal> signal_;
    // Used for non-cancelable operations
    bool *cancelled_ = nullptr;
    bool resumed_ = false;

    ResultType result_;
};

} // namespace corio::detail

namespace asio {

template <typename... Args>
struct async_result<corio::detail::use_corio_t, void(Args...)> {

    template <class Initiation, class... InitArgs>
    static auto initiate(Initiation &&initiation, corio::detail::use_corio_t,
                         InitArgs &&...args) {
        auto packed_init_args =
            std::make_tuple(std::forward<InitArgs>(args)...);
        using PackagedInitArgs = decltype(packed_init_args);
        return corio::detail::Operation<Initiation, PackagedInitArgs, Args...>{
            std::move(initiation), std::move(packed_init_args)};
    }
};

} // namespace asio