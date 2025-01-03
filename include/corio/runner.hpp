#pragma once

#include "corio/lazy.hpp"
#include <asio.hpp>

namespace corio {

inline asio::any_io_executor get_default_executor() noexcept;

template <typename T>
inline T block_on(asio::any_io_executor executor, Lazy<T> lazy);

template <typename T> inline T run(Lazy<T> lazy);

} // namespace corio

#include "corio/impl/runner.ipp"
