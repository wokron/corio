#pragma once

#include "corio/detail/serial_runner.hpp"
#include <mutex>

namespace corio::detail {

struct TaskContext {
    SerialRunner runner;

    // Fields related to TaskSharedState
    std::mutex *mu_ptr;
    SerialRunner *curr_runner_ptr;
};

} // namespace corio::detail