#pragma once

#include <functional>
#include <memory>
#include <mutex>

namespace corio::detail {

template <typename T> class LazySingleton {
public:
    static void init(std::function<std::unique_ptr<T>()> factory) {
        static std::once_flag flag;
        std::call_once(flag, [&] { instance_ = factory(); });
    }

    static T &get() noexcept { return *instance_; }

protected:
    LazySingleton() noexcept = default;

private:
    static std::unique_ptr<T> instance_;
};

template <typename T> std::unique_ptr<T> LazySingleton<T>::instance_ = nullptr;

} // namespace corio::detail