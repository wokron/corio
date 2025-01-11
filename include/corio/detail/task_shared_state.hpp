#pragma once

#include "corio/detail/assert.hpp"
#include "corio/detail/background.hpp"
#include "corio/detail/serial_runner.hpp"
#include "corio/result.hpp"
#include <asio.hpp>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>

namespace corio::detail {

template <typename T>
class TaskSharedState
    : public std::enable_shared_from_this<TaskSharedState<T>> {
public:
    using super = std::enable_shared_from_this<TaskSharedState>;

    explicit TaskSharedState(const SerialRunner &runner)
        : curr_runner_(runner) {
        background_.runner = curr_runner_;
        background_.mu_ptr = &mu_;
        background_.curr_runner_ptr = &curr_runner_;
    }

    template <typename Executor>
    explicit TaskSharedState(const Executor &executor)
        : TaskSharedState(SerialRunner{executor}) {}

public:
    std::lock_guard<std::mutex> lock() { return std::lock_guard(mu_); }

    bool is_finished() const { return entry_handle_ == nullptr; }

    bool is_cancelled() const { return is_finished() && !result_.has_value(); }

    void set_result(Result<T> result) {
        CORIO_ASSERT(!result_.has_value(), "The result is already set");
        result_ = std::move(result);
    }

    Result<T> &result() {
        CORIO_ASSERT(result_.has_value(), "The task is not finished");
        return result_.value();
    }

    void register_resumer(const asio::any_io_executor &await_executor,
                          const std::coroutine_handle<> &await_handle,
                          const std::shared_ptr<bool> &canceled) {
        // This method is called from the executor that awaits the task
        resumer_ = Resumer{await_executor, await_handle, canceled};
    }

    bool request_task_resume() {
        // This method is called from executor that runs the task
        if (!resumer_.has_value()) {
            return false;
        }
        auto &resumer = resumer_.value();
        auto do_resume = [h = resumer.await_handle_,
                          canceled = resumer.canceled_] {
            if (!*canceled) {
                h.resume();
            }
        };
        asio::post(resumer.await_executor_, do_resume);
        resumer_ = std::nullopt;
        return true;
    }

    bool request_abort() {
        // This method is called from executor that awaits the task
        bool is_success_finished = result_.has_value();
        if (is_success_finished || requested_abort_) {
            return false;
        }
        requested_abort_ = true;

        auto task_executor = curr_runner_.get_executor();

        struct AbortChaser {
            std::shared_ptr<TaskSharedState> state;
            asio::any_io_executor prev_task_executor;

            void operator()() {
                auto lock = state->lock();
                if (state->entry_handle_ == nullptr) {
                    return; // The task is already finished
                }
                auto task_executor = state->curr_runner_.get_executor();
                if (task_executor != prev_task_executor) {
                    // The task is switched to another executor. we need to
                    // cancel task on the executor the task is running, so we
                    // post the task to the executor and call this function
                    // again.
                    prev_task_executor = task_executor;
                    asio::post(task_executor, *this);
                    return;
                }
                state->entry_handle_.destroy();
                state->entry_handle_ = nullptr;
                state->request_task_resume();
            }
        };

        asio::post(task_executor,
                   AbortChaser{.state = super::shared_from_this(),
                               .prev_task_executor = task_executor});

        return true;
    }

    Background *background() { return &background_; }

    void set_entry_handle(std::coroutine_handle<> entry_handle) {
        entry_handle_ = entry_handle;
    }

private:
    std::mutex mu_;

    // For task cancellation
    std::coroutine_handle<> entry_handle_;
    bool requested_abort_ = false;
    SerialRunner curr_runner_;

    // For task awaiting
    std::optional<corio::Result<T>> result_;
    struct Resumer {
        asio::any_io_executor await_executor_;
        std::coroutine_handle<> await_handle_;
        std::shared_ptr<bool> canceled_;
    };
    std::optional<Resumer> resumer_;

    // Background for all coroutines in this task
    Background background_;
};

} // namespace corio::detail
