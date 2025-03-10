#pragma once

#include "corio/detail/concepts.hpp"
#include "corio/detail/serial_runner.hpp"
#include "corio/detail/task_shared_state.hpp"
#include "corio/lazy.hpp"
#include "corio/task.hpp"
#include <memory>

namespace corio {

namespace detail {

template <detail::awaitable Awaitable, typename T>
Lazy<void> launch_task(Awaitable aw,
                       std::shared_ptr<detail::TaskSharedState<T>> state) {
    Result<T> result;
    try {
        if constexpr (std::is_void_v<T>) {
            co_await aw;
            result = Result<T>::from_result();
        } else {
            auto r = co_await aw;
            result = Result<T>::from_result(std::move(r));
        }
    } catch (...) {
        result = Result<T>::from_exception(std::current_exception());
    }

    {
        auto lock = state->lock();
        state->set_result(std::move(result));
        state->request_task_resume();
        state->set_entry_handle(nullptr);
    }
}

template <detail::awaitable Awaitable, typename T>
Lazy<void>
launch_task_background(Awaitable aw,
                       std::unique_ptr<detail::TaskSharedState<T>> state) {
    co_await aw;
}

} // namespace detail

template <typename T>
template <detail::awaitable Awaitable>
Task<T>::Task(Awaitable aw, const detail::SerialRunner &runner) {
    state_ = std::make_shared<SharedState>(runner);
    Lazy<void> entry = detail::launch_task(std::move(aw), state_);

    auto entry_handle = entry.get();
    state_->set_entry_handle(entry_handle);
    entry_handle.promise().set_destroy_when_exit(true);

    entry.set_context(state_->context());
    entry.execute(); // After setting entry to prevent a lock
    entry.release();
}

template <typename T> Task<T> &Task<T>::operator=(Task &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    state_ = std::move(other.state_);
    return *this;
}

template <typename T> Task<T>::~Task() {
    if (state_ != nullptr) {
        auto lock = state_->lock();
        if (!state_->is_finished()) {
            state_->request_abort();
        }
    }
}

template <typename T> bool Task<T>::is_finished() const {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    auto lock = state_->lock();
    return state_->is_finished();
}

template <typename T> bool Task<T>::is_cancelled() const {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    auto lock = state_->lock();
    return state_->is_cancelled();
}

template <typename T> Result<T> Task<T>::get_result() {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    auto lock = state_->lock();
    return std::move(state_->result());
}

template <typename T> bool Task<T>::abort() {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    auto lock = state_->lock();
    return state_->request_abort();
}

template <typename T> AbortHandle<T> Task<T>::get_abort_handle() {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    return AbortHandle<T>(state_);
}

template <typename T> bool Task<T>::detach() {
    if (state_ == nullptr) {
        return false;
    }
    state_ = nullptr;
    return true;
}

namespace detail {

template <typename T> class TaskAwaiter {
public:
    using SharedState = typename Task<T>::SharedState;

    explicit TaskAwaiter(const std::shared_ptr<SharedState> &state)
        : state_(state) {}

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        auto lock = state_->lock();

        if (state_->is_finished()) {
            return false;
        }

        Promise &promise = handle.promise();
        auto executor = promise.context()->runner.get_executor();

        state_->register_resumer(executor, handle, canceled_);

        return true;
    }

    T await_resume() {
        corio::Result<T> result;
        {
            auto lock = state_->lock();

            CORIO_ASSERT(state_->is_finished(), "The task is not finished");

            if (state_->is_cancelled()) {
                throw CancellationError("The task is canceled");
            }

            result = std::move(state_->result());
        }

        if constexpr (std::is_void_v<T>) {
            result.result();
            return;
        } else {
            return std::move(result.result());
        }
    }

    ~TaskAwaiter() {
        if (canceled_ != nullptr) {
            *canceled_ = true;
        }
    }

private:
    std::shared_ptr<SharedState> state_;
    std::shared_ptr<bool> canceled_ = std::make_shared<bool>(false);
};

} // namespace detail

template <typename T> auto Task<T>::operator co_await() const {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    return detail::TaskAwaiter<T>(state_);
}

template <typename T> bool AbortHandle<T>::abort() {
    auto lock = state_->lock();
    return state_->request_abort();
}

namespace detail {

template <detail::awaitable Awaitable> class ForkTaskAwaiter {
public:
    explicit ForkTaskAwaiter(Awaitable aw) : aw_(std::move(aw)) {}

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) {
        Promise &promise = handle.promise();
        forked_runner_ = promise.context()->runner.fork_runner();
        return false;
    }

    auto await_resume() {
        using T = detail::awaitable_return_t<Awaitable>;
        return Task<T>(std::move(aw_), forked_runner_);
    }

private:
    Awaitable aw_;
    SerialRunner forked_runner_;
};

template <awaitable Awaitable>
void spawn_background_impl(Awaitable aw, const detail::SerialRunner &runner) {
    using T = detail::awaitable_return_t<Awaitable>;
    auto state = std::make_unique<detail::TaskSharedState<T>>(runner);
    auto state_raw = state.get();
    Lazy<void> entry =
        detail::launch_task_background(std::move(aw), std::move(state));

    auto entry_handle = entry.get();
    entry_handle.promise().set_destroy_when_exit(true);

    entry.set_context(state_raw->context());
    entry.execute();
    entry.release();
}

template <detail::awaitable Awaitable> class ForkTaskBackgroundAwaiter {
public:
    explicit ForkTaskBackgroundAwaiter(Awaitable aw) : aw_(std::move(aw)) {}

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle) {
        Promise &promise = handle.promise();
        forked_runner_ = promise.context()->runner.fork_runner();
        return false;
    }

    void await_resume() {
        using T = detail::awaitable_return_t<Awaitable>;
        spawn_background_impl(std::move(aw_), forked_runner_);
    }

private:
    Awaitable aw_;
    SerialRunner forked_runner_;
};

} // namespace detail

template <detail::awaitable Awaitable>
Lazy<Task<detail::awaitable_return_t<Awaitable>>> spawn(Awaitable aw) {
    co_return co_await detail::ForkTaskAwaiter<Awaitable>(std::move(aw));
}

template <typename Executor, detail::awaitable Awaitable>
Task<detail::awaitable_return_t<Awaitable>> spawn(const Executor &executor,
                                                  Awaitable aw) {
    return Task<detail::awaitable_return_t<Awaitable>>(std::move(aw), executor);
}

template <typename Executor, detail::awaitable Awaitable>
void spawn_background(const Executor &executor, Awaitable aw) {
    detail::spawn_background_impl(std::move(aw),
                                  detail::SerialRunner{executor});
}

template <detail::awaitable Awaitable>
Lazy<void> spawn_background(Awaitable aw) {
    co_await detail::ForkTaskBackgroundAwaiter<Awaitable>(std::move(aw));
}

} // namespace corio