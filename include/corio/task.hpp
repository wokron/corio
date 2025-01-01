#pragma once

#include "asio/any_io_executor.hpp"
#include "corio/lazy.hpp"

namespace corio {

template <typename T> class Task;

template <typename T> class TaskAwaiter;

template <typename T> Lazy<Task<T>> spawn(Lazy<T> lazy);

template <typename T>
Task<T> spawn(asio::any_io_executor executor, Lazy<T> lazy);

template <typename T> class Task {
public:
    explicit Task(Lazy<T> lazy);

public:
    Task();

    Task(Task const &) = delete;

    Task &operator=(Task const &) = delete;

    Task(Task &&other) noexcept;

    Task &operator=(Task &&other) noexcept;

    operator bool() const;

public:
    bool is_finished() const;

    Result<T> &get_result();

    const Result<T> &get_result() const;

    void abort();

    void detach();

    TaskAwaiter<T> operator co_await() const;
};

} // namespace corio