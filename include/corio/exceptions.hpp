#ifndef CORIO_EXCEPTIONS_HPP
#define CORIO_EXCEPTIONS_HPP

#include <stdexcept>

#define CORIO_DEFINE_EXCEPTION(name)                                           \
    class name : public std::runtime_error {                                   \
    public:                                                                    \
        using std::runtime_error::runtime_error;                               \
    }

namespace corio {

CORIO_DEFINE_EXCEPTION(AssertionError);

} // namespace corio

#endif // CORIO_EXCEPTIONS_HPP