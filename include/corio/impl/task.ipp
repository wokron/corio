#pragma once

#include "corio/exceptions.hpp"
#include "corio/task.hpp"
#include "corio/this_coro.hpp"
#include <memory>

namespace corio {

template <typename T>
Task<T>::Task(Lazy<T> lazy, asio::any_io_executor executor) {
    state_ = std::make_shared<SharedState>();
    Lazy<void> entry = launch_task_(std::move(lazy), state_);
    entry.set_executor(executor);
    state_->set_entry(std::move(entry));
    state_->entry().execute(); // After setting entry to prevent a lock
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
        abort();
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

template <typename T> class TaskAwaiter {
public:
    using SharedState = typename Task<T>::SharedState;

    explicit TaskAwaiter(std::shared_ptr<SharedState> state)
        : state_(std::move(state)) {}

    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        auto lock = state_->lock();

        if (state_->is_finished()) {
            return false;
        }

        auto strand = state_->entry().get_strand();
        state_->set_resumer(strand, handle, canceled_);

        return true;
    }

    T await_resume() {
        auto lock = state_->lock();

        CORIO_ASSERT(state_->is_finished(), "The task is not finished");

        if (state_->is_cancelled()) {
            throw CancellationError("The task is canceled");
        }

        auto result = std::move(state_->result());
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

template <typename T> TaskAwaiter<T> Task<T>::operator co_await() const {
    CORIO_ASSERT(state_ != nullptr, "Task is not initialized");
    return TaskAwaiter<T>(state_);
}

namespace detail {

template <typename T> struct TaskLazyAwaiter {
    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    std::coroutine_handle<typename corio::Lazy<T>::promise_type>
    await_suspend(std::coroutine_handle<PromiseType> caller_handle) {
        return lazy.chain_coroutine(caller_handle);
    }

    void await_resume() noexcept {}

    Lazy<T> &lazy;
};

} // namespace detail

template <typename T>
Lazy<void> Task<T>::launch_task_(Lazy<T> lazy,
                                 std::shared_ptr<SharedState> state) {
    detail::TaskLazyAwaiter<T> awaiter{lazy};

    co_await awaiter;
    CORIO_ASSERT(lazy.is_finished(), "The lazy is not finished");

    {
        auto lock = state->lock();

        auto result = std::move(lazy.get_result());
        state->set_result(std::move(result));
        state->resume();

        auto self_handle = state->entry().release();
        CORIO_ASSERT(self_handle, "The handle is null");
        self_handle.promise().set_destroy_when_exit(true);
    }
}

template <typename T>
Task<T> spawn(asio::any_io_executor executor, Lazy<T> lazy) {
    return Task<T>(std::move(lazy), executor);
}

template <typename T> Lazy<Task<T>> spawn(Lazy<T> lazy) {
    // The new task will run concurrently with the current task, so we should
    // use executor instead of strand here
    asio::any_io_executor executor = co_await this_coro::executor;
    co_return spawn(executor, std::move(lazy));
}

template <typename T> bool AbortHandle<T>::abort() {
    CORIO_ASSERT(state_ != nullptr, "The task is not initialized");
    return state_->request_abort();
}

template <typename T> Lazy<void> spawn_background(Lazy<T> lazy) {
    asio::any_io_executor executor = co_await this_coro::executor;
    spawn_background(executor, std::move(lazy));
}

template <typename T>
void spawn_background(asio::any_io_executor executor, Lazy<T> lazy) {
    Task<T> task = spawn(executor, std::move(lazy));
    task.detach();
}

} // namespace corio