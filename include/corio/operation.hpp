#pragma once

#include "corio/detail/operation.hpp"

namespace corio {

struct use_corio_t {
    constexpr use_corio_t() {}

    template <typename InnerExecutor>
    struct executor_with_default : InnerExecutor {
        using default_completion_token_type = use_corio_t;

        executor_with_default(const InnerExecutor &inner_executor)
            : InnerExecutor(inner_executor) {}

        template <typename InnerExecutor1>
        executor_with_default(
            const InnerExecutor1 &ex,
            typename std::enable_if_t<std::conditional_t<
                !std::is_same_v<InnerExecutor1, executor_with_default>,
                std::is_convertible<InnerExecutor1, InnerExecutor>,
                std::false_type>::value> = 0) noexcept
            : InnerExecutor(ex) {}
    };

    template <typename T>
    using as_default_on_t = typename T::template rebind_executor<
        executor_with_default<typename T::executor_type>>::other;

    template <typename T>
    static typename std::decay_t<T>::template rebind_executor<
        executor_with_default<typename std::decay_t<T>::executor_type>>::other
    as_default_on(T &&t) {
        return typename std::decay_t<T>::template rebind_executor<
            executor_with_default<typename std::decay_t<T>::executor_type>>::
            other(std::forward<T>(t));
    }
};

inline constexpr use_corio_t use_corio;

} // namespace corio

namespace asio {

template <typename... Args>
struct async_result<corio::use_corio_t, void(Args...)> {

    template <class Initiation, class... InitArgs>
    static auto initiate(Initiation &&initiation, corio::use_corio_t,
                         InitArgs &&...args) {
        auto packed_init_args =
            std::make_tuple(std::forward<InitArgs>(args)...);
        using PackagedInitArgs = decltype(packed_init_args);
        return corio::detail::Operation<Initiation, PackagedInitArgs, Args...>{
            std::move(initiation), std::move(packed_init_args)};
    }
};

} // namespace asio