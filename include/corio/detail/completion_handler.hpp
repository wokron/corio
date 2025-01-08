#pragma once

#include "corio/detail/type_traits.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <coroutine>
#include <memory>
#include <type_traits>

namespace corio::detail {

inline std::exception_ptr make_exception_ptr(const asio::error_code &ec) {
    return std::make_exception_ptr(asio::system_error(ec));
}

inline auto build_result() { return corio::Result<void>::from_result(); }

inline auto build_result(const asio::error_code &ec) {
    if (ec) {
        return corio::Result<void>::from_exception(make_exception_ptr(ec));
    } else {
        return corio::Result<void>::from_result();
    }
}

template <typename Arg>
requires(!std::is_same_v<std::decay_t<Arg>, asio::error_code>)
inline auto build_result(Arg &&arg) {
    using T = std::decay_t<Arg>;
    return corio::Result<T>::from_result(std::forward<Arg>(arg));
}

template <typename Arg>
inline auto build_result(const asio::error_code &ec, Arg &&arg) {
    using T = std::decay_t<Arg>;
    if (ec) {
        return corio::Result<T>::from_exception(make_exception_ptr(ec));
    } else {
        return corio::Result<T>::from_result(std::forward<Arg>(arg));
    }
}

template <typename Arg, typename... Args>
requires(!std::is_same_v<std::decay_t<Arg>, asio::error_code>)
inline auto build_result(Arg &&arg, Args &&...args) {
    auto t =
        std::make_tuple(std::forward<Arg>(arg), std::forward<Args>(args)...);
    using T = decltype(t);
    return corio::Result<T>::from_result(std::move(t));
}

template <typename... Args>
requires(sizeof...(Args) > 1)
inline auto build_result(const asio::error_code &ec, Args &&...args) {
    auto t = std::make_tuple(std::forward<Args>(args)...);
    using T = decltype(t);
    if (ec) {
        return corio::Result<T>::from_exception(make_exception_ptr(ec));
    } else {
        return corio::Result<T>::from_result(std::move(t));
    }
}

inline bool is_operation_aborted() { return false; }

template <typename Arg, typename... Args>
requires(!std::is_same_v<std::decay_t<Arg>, asio::error_code>)
inline bool is_operation_aborted(Arg &&arg, Args &&...args) {
    return false;
}

template <typename... Args>
inline bool is_operation_aborted(const asio::error_code &ec, Args &&...args) {
    return ec == asio::error::operation_aborted;
}

template <typename... Args> class CompletionHandler {
public:
    using ResultType = decltype(build_result(std::declval<Args>()...));

    explicit CompletionHandler(std::coroutine_handle<> handle,
                               asio::cancellation_slot slot, ResultType &result)
        : handle_(handle), slot_(std::move(slot)), result_(result) {}

public:
    using cancellation_slot_type = asio::cancellation_slot;

    cancellation_slot_type get_cancellation_slot() const { return slot_; }

public:
    void operator()(Args... args) const {
        if (is_operation_aborted(args...)) {
            return;
        }
        result_ = build_result(std::forward<Args>(args)...);
        handle_.resume();
    }

private:
    std::coroutine_handle<> handle_;
    asio::cancellation_slot slot_;
    ResultType &result_;
};

} // namespace corio::detail