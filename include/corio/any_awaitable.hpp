#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include "corio/lazy.hpp"
#include <memory>
#include <optional>
#include <variant>

namespace corio {

template <typename... Return> class AnyAwaitable {
public:
    using ReturnType = detail::unique_safe_simplified_variant_t<Return...>;

    AnyAwaitable() = default;

    template <detail::awaitable Awaitable> AnyAwaitable(Awaitable &&awaitable);

    operator bool() const { return self_.index() != 0; }

    auto operator co_await();

private:
    using Deleter = void (*)(void *);
    using Invoker = Lazy<ReturnType> (*)(void *self);

    template <detail::awaitable Awaitable>
    static auto make_void_unique_ptr_(Awaitable &&awaitable);

    std::variant<std::monostate, std::unique_ptr<void, Deleter>, void *> self_;
    Invoker invoker_;
    std::optional<Lazy<ReturnType>> lazy_ = std::nullopt;
};

} // namespace corio

#include "corio/impl/any_awaitable.ipp"