#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include <asio.hpp>

namespace corio {

inline asio::any_io_executor get_default_executor() noexcept;

template <typename Executor, detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> block_on(const Executor &executor,
                                                      Awaitable aw);

template <detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> run(Awaitable aw);

} // namespace corio

#include "corio/impl/run.ipp"