#ifndef CORIO_IMPL_RESULT_IPP
#define CORIO_IMPL_RESULT_IPP

#include "corio/result.hpp"

namespace corio {

template <typename T>
template <typename U>
    requires(!std::is_void_v<U>)
inline Result<std::decay_t<U>> Result<T>::from_result(U &&result) {
    Result<std::decay_t<U>> result_;
    result_.set_result_(std::forward<U>(result));
    return result_;
}

template <typename T>
template <typename U>
    requires(std::is_void_v<U>)
inline Result<U> Result<T>::from_result() {
    Result<U> result_;
    result_.set_result_();
    return result_;
}

template <typename T>
inline Result<T>
Result<T>::from_exception(const std::exception_ptr &exception) {
    Result result_;
    result_.set_exception_(exception);
    return result_;
}

template <typename T>
template <typename U>
    requires(!std::is_void_v<U>)
inline U &Result<T>::result() {
    if (value_.index() == RESULT_OK) {
        return std::get<RESULT_OK>(value_);
    }
    std::rethrow_exception(std::get<RESULT_ERR>(value_));
}

template <typename T>
template <typename U>
    requires(!std::is_void_v<U>)
inline const U &Result<T>::result() const {
    if (value_.index() == RESULT_OK) {
        return std::get<RESULT_OK>(value_);
    }
    std::rethrow_exception(std::get<RESULT_ERR>(value_));
}

template <typename T>
template <typename U>
    requires(std::is_void_v<U>)
inline void Result<T>::result() const {
    if (value_.index() == RESULT_ERR) {
        std::rethrow_exception(std::get<RESULT_ERR>(value_));
    }
}

template <typename T> inline std::exception_ptr Result<T>::exception() const {
    if (value_.index() == RESULT_ERR) {
        return std::get<RESULT_ERR>(value_);
    }
    return nullptr;
}

template <typename T>
template <typename U>
    requires(!std::is_void_v<U>)
inline void Result<T>::set_result_(U &&result) {
    value_ = ValueType{std::in_place_index<RESULT_OK>, std::forward<U>(result)};
}

template <typename T>
template <typename U>
    requires(std::is_void_v<U>)
inline void Result<T>::set_result_() {
    value_ = ValueType{std::in_place_index<RESULT_OK>, std::monostate{}};
}

template <typename T>
inline void Result<T>::set_exception_(const std::exception_ptr &exception) {
    value_ = ValueType{std::in_place_index<RESULT_ERR>, exception};
}

} // namespace corio

#endif // CORIO_IMPL_RESULT_IPP