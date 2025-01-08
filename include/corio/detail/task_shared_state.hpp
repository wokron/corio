#pragma once

#include "corio/detail/assert.hpp"
#include "corio/exceptions.hpp"
#include "corio/lazy.hpp"
#include "corio/result.hpp"
#include "corio/this_coro.hpp"
#include <asio.hpp>
#include <coroutine>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace corio::detail {

template <typename T>
class TaskSharedState
    : public std::enable_shared_from_this<TaskSharedState<T>> {
public:
    using super = std::enable_shared_from_this<TaskSharedState<T>>;

    TaskSharedState() {}

    std::lock_guard<std::mutex> lock() { return std::lock_guard(mu_); }

    void set_entry(corio::Lazy<void> entry) {
        CORIO_ASSERT(!entry_, "The entry is already set");
        entry_ = std::move(entry);
    }

    Lazy<void> &entry() { return entry_; }

    bool request_abort() {
        bool is_success_finished = result_.has_value();
        if (is_success_finished || requested_abort_) {
            return false;
        }
        requested_abort_ = true;

        asio::post(entry_.get_strand(), [state = super::shared_from_this()] {
            std::lock_guard lock = state->lock();
            state->entry().reset();
            state->resume();
        });

        return true;
    }

    bool is_finished() const {
        bool entry_is_valid = entry_;
        return !entry_is_valid;
    }

    bool is_cancelled() const { return is_finished() && !result_.has_value(); }

    void set_result(Result<T> result) {
        CORIO_ASSERT(!result_.has_value(), "The result is already set");
        result_ = std::move(result);
    }

    Result<T> &result() {
        CORIO_ASSERT(result_.has_value(), "The task is not finished");
        return result_.value();
    }

    bool resume() {
        if (!resumer_.has_value()) {
            return false;
        }
        auto &resumer = resumer_.value();
        auto do_resume = [h = resumer.handle_, c = resumer.canceled_] {
            bool is_canceled = *c;
            if (!is_canceled) {
                h.resume();
            }
        };
        asio::post(resumer.strand_, do_resume);
        resumer_ = std::nullopt;
        return true;
    }

    void set_resumer(const asio::strand<asio::any_io_executor> &strand,
                     std::coroutine_handle<> handle,
                     std::shared_ptr<bool> canceled) {
        resumer_ = Resumer{strand, handle, canceled};
    }

private:
    std::mutex mu_;

    corio::Lazy<void> entry_;
    bool requested_abort_ = false;

    std::optional<Result<T>> result_;

    // TODO: extract this struct as a common class, and use it in other places
    struct Resumer {
        asio::strand<asio::any_io_executor> strand_;
        std::coroutine_handle<> handle_;
        std::shared_ptr<bool> canceled_;
    };
    std::optional<Resumer> resumer_;
};

} // namespace corio::detail