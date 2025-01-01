#pragma once

#include <functional>

namespace corio::detail {

class DeferGuard {
public:
    DeferGuard(const std::function<void()> &fn) : fn_(fn) {}

    ~DeferGuard() { fn_(); }

private:
    std::function<void()> fn_;
};

} // namespace corio::detail