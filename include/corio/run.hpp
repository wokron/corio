#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/lazy.hpp"
#include <asio.hpp>

namespace corio {

inline asio::any_io_executor get_default_executor() noexcept;

template <detail::awaitable Awaitable,
          typename Return = detail::awaitable_return_t<Awaitable>>
inline Return block_on(asio::any_io_executor executor, Awaitable &&awaitable);

template <detail::awaitable Awaitable,
          typename Return = detail::awaitable_return_t<Awaitable>>
inline Return run(Awaitable &&awaitable);

} // namespace corio

#include "corio/impl/run.ipp"
