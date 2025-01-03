#pragma once

#include <memory>
#include <mutex>

namespace corio::detail {

template <typename T> class LazySingleton {
public:
    template <typename... Args> static void init(Args &&...args) {
        static std::once_flag flag;
        std::call_once(flag, [&] {
            instance_ = std::make_unique<T>(std::forward<Args>(args)...);
        });
    }

    static T &get() noexcept { return *instance_; }

protected:
    LazySingleton() noexcept = default;

private:
    static std::unique_ptr<T> instance_;
};

template <typename T> std::unique_ptr<T> LazySingleton<T>::instance_ = nullptr;

} // namespace corio::detail