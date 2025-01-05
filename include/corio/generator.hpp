#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/generator_promise.hpp"
#include <bits/iterator_concepts.h>
#include <coroutine>
#include <utility>

namespace corio {

template <typename T> class Generator {
public:
    using promise_type = detail::GeneratorPromise<T>;

    Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

public:
    Generator(const Generator &) = delete;
    Generator &operator=(const Generator &) = delete;

    Generator(Generator &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    Generator &operator=(Generator &&other) noexcept;

    ~Generator();

    operator bool() const { return handle_ != nullptr; }

    template <typename PromiseType>
    std::coroutine_handle<promise_type>
    chain_coroutine(std::coroutine_handle<PromiseType> caller_handle);

public:
    auto operator co_await() noexcept;

    bool has_current() const;

    T current();

    auto get_strand() const;

private:
    std::coroutine_handle<promise_type> handle_ = nullptr;
};

#define async_for(decl, gen)                                                   \
    if (auto _gen = (gen); true)                                               \
        for (bool _more = co_await _gen; _more; _more = co_await _gen)         \
            if (decl = _gen.current(); true)

} // namespace corio

#include "corio/impl/generator.ipp"