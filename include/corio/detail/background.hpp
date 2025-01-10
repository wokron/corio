#pragma once

#include "corio/detail/serial_runner.hpp"

namespace corio::detail {

struct Background {
    SerialRunner runner;
};

} // namespace corio::detail