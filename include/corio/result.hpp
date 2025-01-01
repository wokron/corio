#ifndef CORIO_RESULT_HPP
#define CORIO_RESULT_HPP

#include "corio/detail/type_traits.hpp"
#include <exception>
#include <type_traits>
#include <variant>

namespace corio {

template <typename T> class Result {
public:
    template <typename U = T>
        requires(!std::is_void_v<U>)
    static Result<std::decay_t<U>> from_result(U &&result);

    template <typename U = T>
        requires(std::is_void_v<U>)
    static Result<U> from_result();

    static Result from_exception(const std::exception_ptr &exception);

    template <typename U = T>
        requires(!std::is_void_v<U>)
    U &result();

    template <typename U = T>
        requires(!std::is_void_v<U>)
    const U &result() const;

    template <typename U = T>
        requires(std::is_void_v<U>)
    void result() const;

    std::exception_ptr exception() const;

private:
    static constexpr int RESULT_OK = 1;
    static constexpr int RESULT_ERR = 2;

    template <typename U = T>
        requires(!std::is_void_v<U>)
    void set_result_(U &&result);

    template <typename U = T>
        requires(std::is_void_v<U>)
    void set_result_();

    void set_exception_(const std::exception_ptr &exception);

    using ValueType =
        std::variant<std::monostate, detail::void_to_monostate_t<T>,
                     std::exception_ptr>;
    ValueType value_;
};

} // namespace corio

#include "corio/impl/result.ipp"

#endif // CORIO_RESULT_HPP