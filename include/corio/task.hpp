#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/task_shared_state.hpp"
#include "corio/lazy.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <coroutine>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace corio {

template <typename T> class Task;

template <typename T> [[nodiscard]] Lazy<Task<T>> spawn(Lazy<T> lazy);

template <typename T>
[[nodiscard]] Task<T> spawn(asio::any_io_executor executor, Lazy<T> lazy);

template <typename T> Lazy<void> spawn_background(Lazy<T> lazy);

template <typename T>
void spawn_background(asio::any_io_executor executor, Lazy<T> lazy);

template <typename T> class TaskAwaiter;

template <typename T> class AbortHandle;

template <typename T> class Task {
public:
    using SharedState = detail::TaskSharedState<T>;

    explicit Task(Lazy<T> lazy, asio::any_io_executor executor);

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

    TaskAwaiter<T> operator co_await() const;

private:
    static Lazy<void> launch_task_(Lazy<T> lazy,
                                   std::shared_ptr<SharedState> shared_state);

    std::shared_ptr<SharedState> state_ = nullptr;
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