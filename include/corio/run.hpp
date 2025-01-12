#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/type_traits.hpp"
#include <asio.hpp>

namespace corio {

template <typename Executor, detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> block_on(const Executor &executor,
                                                      Awaitable aw);

template <detail::awaitable Awaitable>
inline detail::awaitable_return_t<Awaitable> run(Awaitable aw,
                                                 bool multi_thread = true);

} // namespace corio

#include "corio/impl/run.ipp"