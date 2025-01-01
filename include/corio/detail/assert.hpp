#pragma once

#include "corio/exceptions.hpp"
#include <iostream>
#include <source_location>
#include <sstream>
#include <string>

namespace corio::detail {

[[noreturn]] inline void handle_assert(const std::string &expr,
                                       std::source_location loc,
                                       const std::string &msg = "") {
    std::ostringstream oss;
    oss << "Assertion failed: " << expr << " in " << loc.file_name() << ":"
        << loc.line() << " " << loc.function_name();
    if (!msg.empty()) {
        oss << " " << msg;
    }
    throw AssertionError(oss.str());
}

inline void handle_warning(const std::string &expr, std::source_location loc,
                           const std::string &msg = "") {
    std::cerr << "Warning: " << expr << " in " << loc.file_name() << ":"
              << loc.line() << " " << loc.function_name();
    if (!msg.empty()) {
        std::cerr << " " << msg;
    }
    std::cerr << std::endl;
}

#define CORIO_ASSERT(expr, ...)                                                \
    do {                                                                       \
        if (!(expr)) {                                                         \
            ::corio::detail::handle_assert(                                    \
                #expr, std::source_location::current(), ##__VA_ARGS__);        \
        }                                                                      \
    } while (false)

#define CORIO_WARNING(expr, ...)                                               \
    do {                                                                       \
        if (!(expr)) {                                                         \
            ::corio::detail::handle_warning(                                   \
                #expr, std::source_location::current(), ##__VA_ARGS__);        \
        }                                                                      \
    } while (false)

} // namespace corio::detail