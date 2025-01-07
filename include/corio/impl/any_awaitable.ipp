#pragma once

#include "corio/any_awaitable.hpp"
#include "corio/detail/assert.hpp"
#include <utility>

namespace corio {

template <typename... Return>
template <detail::awaitable Awaitable>
AnyAwaitable<Return...>::AnyAwaitable(Awaitable &&awaitable) {
    if constexpr (std::is_lvalue_reference_v<Awaitable>) {
        // If pass by reference, any awaitable will not take the ownership
        self_ = &awaitable;
    } else {
        // If pass by value, any awaitable will take the ownership
        self_ = make_void_unique_ptr_(std::forward<Awaitable>(awaitable));
    }
    invoker_ = [](void *self) -> Lazy<ReturnType> {
        constexpr bool need_monostate =
            std::is_void_v<detail::awaitable_return_t<Awaitable>> &&
            !std::is_void_v<ReturnType>;

        auto &self_ref = *reinterpret_cast<Awaitable *>(self);
        if constexpr (need_monostate) {
            co_await self_ref;
            co_return std::monostate{};
        } else {
            co_return co_await self_ref;
        }
    };
}

template <typename... Return>
auto AnyAwaitable<Return...>::operator co_await() {
    CORIO_ASSERT(self_.index() != 0, "Invalid state");
    if (auto *self_ptr = std::get_if<void *>(&self_)) {
        lazy_ = invoker_(*self_ptr);
    } else {
        auto &self = std::get<std::unique_ptr<void, Deleter>>(self_);
        lazy_ = invoker_(self.get());
    }
    return lazy_->operator co_await();
}

template <typename... Return>
template <detail::awaitable Awaitable>
auto AnyAwaitable<Return...>::make_void_unique_ptr_(Awaitable &&awaitable) {
    auto deleter = [](void *ptr) { delete reinterpret_cast<Awaitable *>(ptr); };
    return std::unique_ptr<void, Deleter>(
        new Awaitable(std::forward<Awaitable>(awaitable)), deleter);
}

} // namespace corio