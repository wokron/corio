#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/serial_runner.hpp"
#include "corio/detail/task_shared_state.hpp"
#include "corio/detail/type_traits.hpp"

namespace corio {

template <typename T> class Task;
template <typename T> class Lazy;

template <detail::awaitable Awaitable>
[[nodiscard]] Lazy<Task<detail::awaitable_return_t<Awaitable>>>
spawn(Awaitable aw);

template <typename Executor, detail::awaitable Awaitable>
[[nodiscard]] Task<detail::awaitable_return_t<Awaitable>>
spawn(const Executor &executor, Awaitable aw);

template <detail::awaitable Awaitable>
Lazy<void> spawn_background(Awaitable aw);

template <typename Executor, detail::awaitable Awaitable>
void spawn_background(const Executor &executor, Awaitable aw);

template <typename T> class AbortHandle;

template <typename T> class [[nodiscard]] Task {
public:
    using SharedState = detail::TaskSharedState<T>;

    template <detail::awaitable Awaitable>
    explicit Task(Awaitable aw, const detail::SerialRunner &runner);

    template <detail::awaitable Awaitable, typename Executor>
    explicit Task(Awaitable aw, const Executor &executor)
        : Task(std::move(aw), detail::SerialRunner{executor}) {}

public:
    Task() = default;

    Task(Task const &) = delete;

    Task &operator=(Task const &) = delete;

    Task(Task &&other) noexcept = default;

    Task &operator=(Task &&other) noexcept;

    operator bool() const { return state_ != nullptr; }

    ~Task();

public:
    bool is_finished() const;

    bool is_cancelled() const;

    Result<T> get_result();

    bool abort();

    AbortHandle<T> get_abort_handle();

    bool detach();

    auto operator co_await() const;

private:
    template <detail::awaitable Awaitable>
    static Lazy<void> launch_task_(Awaitable aw,
                                   std::shared_ptr<SharedState> state);

private:
    std::shared_ptr<SharedState> state_;
};

template <typename T> class AbortHandle {
public:
    using SharedState = typename Task<T>::SharedState;

    explicit AbortHandle(std::shared_ptr<SharedState> state) : state_(state) {}

    bool abort();

private:
    std::shared_ptr<SharedState> state_;
};

} // namespace corio

#include "corio/impl/task.ipp"