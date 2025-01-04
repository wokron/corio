#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include <optional>
#include <type_traits>
#include <utility>
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

    Lazy<ReturnType> do_co_await() {
        return std::visit(
            [](auto &&arg) -> Lazy<ReturnType> {
                using Arg = std::decay_t<decltype(arg)>;
                static_assert(detail::awaitable<Arg>, "Arg is not awaitable");
                if constexpr (std::is_void_v<ReturnType>) {
                    co_await arg;
                    co_return;
                } else if constexpr (std::is_void_v<
                                         detail::awaitable_return_t<Arg>>) {
                    co_await arg;
                    co_return std::monostate{};
                } else {
                    co_return co_await arg;
                }
            },
            *awaitable_);
    }

    auto operator co_await() {
        lazy_ = do_co_await(); // Wrap everything in a Lazy
        return lazy_.value().operator co_await();
    }

    std::variant<Awaitables...> unwrap() {
        return std::exchange(awaitable_, std::nullopt).value();
    }

private:
    std::optional<Lazy<ReturnType>> lazy_; // Keep the lifetime of the Lazy
    std::optional<std::variant<Awaitables...>> awaitable_;
};

} // namespace corio