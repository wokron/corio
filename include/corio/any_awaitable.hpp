#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include <optional>
#include <variant>

namespace corio {

template <detail::awaitable... Awaitables> class AnyAwaitable {
public:
    static_assert((!std::is_reference_v<Awaitables> && ...),
                  "Reference type is not allowed");

    using ReturnType = detail::unique_safe_simplified_variant_t<
        detail::awaitable_return_t<Awaitables>...>;

    AnyAwaitable() = default;

    template <detail::awaitable Awaitable>
    explicit AnyAwaitable(Awaitable &&awaitable)
        : awaitable_(std::forward<Awaitable>(awaitable)) {}

    template <detail::awaitable Awaitable>
    AnyAwaitable &operator=(Awaitable &&awaitable) {
        awaitable_ = std::forward<Awaitable>(awaitable);
        return *this;
    }

    operator bool() const { return awaitable_.has_value(); }

public:
    Lazy<ReturnType> do_co_await();

    auto operator co_await();

    std::variant<Awaitables...> unwrap();

private:
    std::optional<Lazy<ReturnType>> lazy_; // Keep the lifetime of the Lazy
    std::optional<std::variant<Awaitables...>> awaitable_;
};

} // namespace corio

#include "corio/impl/any_awaitable.ipp"