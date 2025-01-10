#pragma once

#include <chrono>
#include <memory>

namespace marker {

template <typename Ret = std::chrono::milliseconds, typename Clock,
          typename Dur>
auto since(const std::chrono::time_point<Clock, Dur> &start) -> Ret {
    return std::chrono::duration_cast<Ret>(Clock::now() - start);
}

template <typename Clock = std::chrono::steady_clock> class Timer {
public:
    Timer() = default;

    void tick() { start_ = Clock::now(); }

    void tock() { end_ = Clock::now(); }

    template <typename Ret = std::chrono::milliseconds>
    auto duration() const -> Ret {
        return std::chrono::duration_cast<Ret>(end_ - start_);
    }

private:
    using TimePoint = std::chrono::time_point<Clock>;
    TimePoint start_, end_;
};

template <typename Clock = std::chrono::steady_clock,
          typename Dur = std::chrono::milliseconds>
class ScopeTimer {
public:
    using DurationPtr = std::shared_ptr<Dur>;

    ScopeTimer() : duration_(std::make_shared<Dur>()) { timer_.tick(); }

    ScopeTimer(DurationPtr duration) : duration_(duration) { timer_.tick(); }

    ~ScopeTimer() {
        timer_.tock();
        *duration_ = timer_.template duration<Dur>();
    }

    DurationPtr duration() const { return duration_; }

private:
    Timer<Clock> timer_;
    DurationPtr duration_;
};

template <typename Dur = std::chrono::milliseconds,
          typename Clock = std::chrono::steady_clock, typename Fn>
auto measured(Fn &&fn) {
    return [fn = std::forward<Fn>(fn)](auto &&...args) {
        auto start = Clock::now();

        using Ret = decltype(fn(std::forward<decltype(args)>(args)...));
        if constexpr (std::is_void_v<Ret>) {
            fn(std::forward<decltype(args)>(args)...);
            return since<Dur>(start);
        } else {
            auto ret = fn(std::forward<decltype(args)>(args)...);
            return std::make_pair(ret, since<Dur>(start));
        }
    };
}

} // namespace marker