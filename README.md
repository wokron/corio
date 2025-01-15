# Corio

**Corio** is a lightweight C++20 coroutine library based on Asio. It provides easy-to-use advanced coroutine features on top of Asio. Corio **seamlessly integrates with Asio**, offering **multi-threaded runtime support** and **flexible coroutine control interfaces**.

## Install

Corio is a header-only library. Therefore, you only need to place the header files from the `include/` directory of this repository in a specified location in your project and add that location to the compiler's include path.

Take CMake as example. To keep the project modular, you can choose to add this project as a git submodule.


```shell
git submodule add https://github.com/wokron/corio.git ./your/path/to/corio
```

Then include Corio in your `CMakeLists.txt`

```cmake
add_library(corio INTERFACE)
target_include_directories(corio INTERFACE ./your/path/to/corio/include)
target_link_libraries(corio INTERFACE ${ASIO_LIBRARY})
```

> [!NOTE]
> As mentioned earlier, Corio depends on Asio. Therefore, to use Corio, you also need to install Asio (non-Boost version).

Finally, include the `corio.hpp` header file in your code to use Corio. All functionalities of Corio are under the `corio::` namespace.

```cpp
#include <corio.hpp>
```

## Usage

### Coroutine Types

#### lazy

`corio::Lazy<T>` is the core of the Corio library. By setting the return value of a function to `Lazy<T>`, we define the function as a coroutine. In the coroutine defined by `Lazy<T>`, instead of using `return`, we use `co_return` to return from the coroutine. Here, `T` is the return type of the coroutine.

```cpp
corio::Lazy<void> f1() {
    co_return;
}

auto f2 = []() -> corio::Lazy<int> {
    co_return 1;
}
```

`Lazy` is **lazy**, which means that when we "call" a function decorated with `Lazy`, the function does not execute immediately but returns an instance of the `Lazy` type. The coroutine runs only when the `co_await` operator is used on the instance, similar to a regular function call.


```cpp
corio::Lazy<int> f();

corio::Lazy<void> f2() {
    int r1 = co_await f();
    
    auto lazy = f();
    int r2 = co_await lazy;
}
```

> [!NOTE]
> You can only use co_await to call another coroutine within a coroutine.

#### generator

The iterator `corio::Generator<T>` is another coroutine type in Corio. In `Generator<T>`, you can use the `co_yield` keyword to return a value of type `T`. After returning, the `Generator` is not destroyed and can resume execution from the last return point until it encounters `co_return`.

Like `Lazy`, `Generator` is also lazy. We need to use `co_await` to resume the execution of the `Generator`. At this point, the return type of `co_await` is `bool`, indicating whether a new `co_yield` was encountered during this execution. If a new `co_yield` is encountered, you can get the latest `co_yield` return value from the `gen.current()` method.

It is straightforward to iterate over a `Generator`.

```cpp
corio::Generator<int> func_gen();

auto gen = func_gen();
while (co_await gen) {
    int v = gen.current();
}
```

Corio also provides the macro `CORIO_ASYNC_FOR()` and the coroutine function `async_for_each()` to simplify the iteration of generators.

```cpp
CORIO_ASYNC_FOR(int v, func_gen()) {
    // ...
}

co_await corio::async_for_each(func_gen(), [](int v) {
    // ...
});
```

### Coroutine Entry

#### run

Coroutines can only be called by other coroutines, so a coroutine entry is needed above all coroutines to separate synchronous and asynchronous programs. `corio::run()` provides a ready-to-use coroutine entry. This function accepts and runs a coroutine of type `Lazy`, blocking until the coroutine finishes. If the coroutine has a return value, the `run()` function will also return it.

By default, the `run()` function starts **multiple threads** based on the number of CPU cores to handle the execution of asynchronous programs. However, you can set `multi_thread = false` to use a single-threaded runtime.

```cpp
corio::Lazy<int> f();

int r = corio::run(f());
// int r = corio::run(f(), /*multi_thread=*/false);
```

#### block_on

If you want to set up the runtime yourself, you can use the `corio::block_on()` function. The first parameter of this function should be an executor that can be converted to `asio::any_io_executor`. The user should ensure that the program execution on this `executor` is **serial**. Some possible `executor`s include:

- The `executor` of an `asio::io_context` running on a single thread.
- The `executor` of an `asio::thread_pool` with a thread count of 1.
- The `executor` of an `asio::io_context` running on multiple threads, wrapped in an `asio::strand`.
- The executor of an `asio::thread_pool` with a thread count greater than 1, wrapped in an `asio::strand`.
- The above executors wrapped in an `asio::any_io_executor`.

```cpp
corio::Lazy<void> f();

asio::io_context io_context;
std::jthread t([&]() { io_context.run(); });
corio::block_on(io_context.get_executor(), f()); // ok

asio::thread_pool pool(1);
corio::block_on(pool.get_executor(), f()); // ok

asio::thread_pool pool(4);
corio::block_on(asio::make_strand(pool.get_executor()), f()); // ok

asio::thread_pool pool(4);
corio::block_on(pool.get_executor(), f()); // wrong
```

> [!WARNING]
> If your `executor` is a multi-threaded executor wrapped in a `strand`, it is best not to wrap it in an `any_io_executor` before passing it to `block_on()`, otherwise the coroutine runtime will degrade to single-threaded.

### Concurrency

#### spawn

`corio::spawn()` and `corio::spawn_background()` are used to create and run a **task**. A task is a set of coroutines executed sequentially. In a coroutine, the role of a task is similar to that of a thread in a synchronous program. The execution of different tasks is concurrent (and even parallel for Corio!!).

The difference between `spawn()` and `spawn_background()` is that `spawn()` returns a non-discardable `corio::Task<T>` instance. This instance can be used to control the task created by `spawn()`. See the next section for details.

`spawn()` and `spawn_background()` provide both non-coroutine and coroutine versions. The non-coroutine version has the same function signature as `block_on()`. The difference is that calling `spawn()` and `spawn_background()` does not block the code until the task ends.


```cpp
corio::Lazy<void> f();
corio::Lazy<void> g();

asio::thread_pool pool(4);

auto t = corio::spawn(asio::make_strand(pool.get_executor()), f());
corio::spawn_background(asio::make_strand(pool.get_executor()), g());
// Here f() and g() are running in parallel
pool.join();
```

When you need to create concurrent tasks within a coroutine, use the coroutine versions of `spawn()` and `spawn_background()`. These versions no longer require passing an `executor`; Corio will automatically decide the runtime for the new task based on the current coroutine's runtime.


```cpp
corio::Lazy<void> f();
corio::Lazy<void> g();

corio::Lazy<void> h() {
    auto t = co_await corio::spawn(f());
    co_await corio::spawn_background(g());
    // Here f(), g() and h() are running in parallel
    // ...
    co_return;
}

corio::run(h());
```

#### task

As mentioned earlier, calling `spawn()` returns a `corio::Task<T>` instance, where `T` is consistent with the return value of the coroutine. You can use `co_await` to wait for the `Task` object. At this point, the current coroutine will suspend until the coroutine corresponding to the `Task` object completes.


```cpp
corio::Lazy<int> f();

corio::Lazy<void> g() {
    corio::Task<int> task = co_await corio::spawn(f());
    int r = co_await task;
    // ...
}
```

Calling the `task.abort()` function can cancel the corresponding task. After calling this function, the corresponding task will be canceled the next time it suspends. All objects on the coroutine stack will be destructed.

Calling the `task.get_abort_handle()` function will return a `corio::AbortHandle<T>` instance. Calling the `abort()` method of this instance can also cancel the corresponding task. If the `task` is being awaited at this time, a `corio::CancellationError` exception will be thrown.

```cpp
corio::Lazy<int> f();

corio::Lazy<void> g(corio::AbortHandle<T> h) {
    h.abort();
    co_return;
}

corio::Lazy<void> h() {
    corio::Task<int> task = co_await corio::spawn(f());
    co_await corio::spawn_background(g(task.get_abort_handle()));
    try {
        int r = co_await task;
    } catch (const corio::CancellationError& e) {
        // ...
    }
    // ...
}
```

> [!WARNING]
> Note that when the `Task<T>` instance is destructed, it will **cancel the corresponding task by default**. Therefore, if you do not want the task to be canceled, you should use `spawn_background()`. Alternatively, you can relinquish control of the task by calling the `task.detach()` method.

### Awaitable Objects

#### awaitable

**Awaitable objects** are objects that satisfy the `awaitable` concept. In Corio, most objects that can be applied with `co_await` are awaitable objects.

```cpp
template <typename Awaiter, typename Promise = void>
concept awaiter =
    requires(Awaiter awaiter, std::coroutine_handle<Promise> handle) {
        { awaiter.await_ready() };
        { awaiter.await_suspend(handle) };
        { awaiter.await_resume() };
    };

template <typename Awaitable, typename Promise = void>
concept awaitable = awaiter<Awaitable, Promise>
    || requires(Awaitable awaitable) {
        { awaitable.operator co_await() };
    }
    || requires(Awaitable awaitable) {
        { operator co_await(awaitable) };
    };
```

#### any_awaitable

`corio::AnyAwaitable<Ts...>` can accept any awaitable object, as long as its return value after applying `co_await` is one of the types in `Ts...`.

If there are multiple types in `Ts...`, the return type of `AnyAwaitable` after applying `co_await` is `std::variant`. If `Ts...` contains `void`, it is replaced with `std::monostate`; if the same type appears multiple times in `Ts...`, only the first occurrence is retained.


```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();

corio::AnyAwaitable<void, int> aw1 = f();
auto r1 = std::get<int>(co_await aw1);

corio::AnyAwaitable<void, int> aw2 = g();
auto r2 = std::get<std::monostate>(co_await aw2);

corio::AnyAwaitable<void, int> aw3 = co_await corio::spawn(f());
auto r3 = std::get<int>(co_await aw3);
```

### Coroutine Control

#### executor

You can apply `co_await` to `corio::this_coro::executor` to get the executor on which the current coroutine is running. The return type is `asio::any_io_executor`. The returned executor can be passed to Asio's IO objects.


```cpp
corio::Lazy<void> f() {
    auto ex = co_await corio::this_coro::executor;
    asio::steady_timer timer(ex, 100us);
    // ...
}
```

> [!WARNING]
> Do not pass this `executor` to `spawn()` or `spawn_background()`. For multi-threaded runtimes, this will cause the newly created task and the current task to run on the same `strand`.

#### yield

You can apply `co_await` to `corio::this_coro::yield` to voluntarily yield the current task. Alternatively, you can achieve the same functionality by calling the `corio::this_coro::do_yield()` function. `yield` is more efficient but is not an awaitable object; the latter returns an awaitable object. Use `yield` unless you have a specific need.

```cpp
corio::Lazy<void> f() {
    co_await corio::this_coro::yield;

    auto aw = corio::this_coro::do_yield();
    co_await aw;
}
```

#### sleep

You can apply `co_await` to `std::chrono::duration` or `std::chrono::time_point` objects to wait for a specific time. Alternatively, you can achieve the same functionality by calling the `corio::this_coro::sleep_for()` and `corio::this_coro::sleep_until()` functions. The difference is similar to `yield`.


```cpp
using namespace std::chrono_literals;

corio::Lazy<void> f() {
    co_await 1s;
    co_await (std::chrono::steady_clock::now() + 1s);

    auto aw1 = corio::this_coro::sleep_for(1s);
    co_await aw1;
    auto aw2 = corio::this_coro::sleep_until(std::chrono::steady_clock::now() + 1s);
    co_await aw2;
}
```

You can also apply `co_await` to various timers in Asio (such as `asio::steady_timer`).

```cpp
using namespace std::chrono_literals;

corio::Lazy<void> f() {
    auto ex = co_await corio::this_coro::executor;
    asio::steady_timer timer(ex, 1s);
    co_await timer; // sleep 1s
    timer.expires_after(1s);
    co_await timer; // sleep 1s
}
```

#### roam

You can apply `co_await` to an `executor` object to switch the current coroutine task to a new runtime. The requirements for the executor are the same as for the `block_on()` function. This helps to choose a more suitable executor for the IO-bound and CPU-bound parts of the coroutine task. Additionally, you can achieve the same functionality by calling the `corio::this_coro::roam_to()` function. The difference is similar to that of `yield`.

```cpp
corio::Lazy<void> io_bound_func();
void cpu_bound_func();

asio::thread_pool p1(1);
asio::thread_pool p2(16);

auto f = [&]() -> corio::Lazy<void> {
    co_await io_bound_func();

    co_await asio::make_strand(p2.get_executor());

    cpu_bound_func(); // Run cpu-bound code in multi-thread runtime

    auto aw = corio::this_coro::roam_to(p1.get_executor());
    co_await aw;

    co_await io_bound_func();
}

corio::block_on(p1.get_executor(), f());
```

#### future

You can apply `co_await` to a `std::future` object to asynchronously wait for the future to complete. This does not block any threads in the current runtime. The return value of `co_await` is the same as the return value of `future.get()`.

```cpp
corio::Lazy<void> f() {
    std::future<int> fut = std::async(std::launch::async, []() -> int { 
        std::this_thread::sleep_for(10s);
        return 42;
    });
    int r = co_await fut;
    // ...
}
```

> [!NOTE]
> `co_await future` does not block any threads in the current runtime because the blocking is transferred to the threads of `asio::system_executor`.

### Synchronization

#### gather

The `corio::gather()` function can wait for multiple awaitable objects in parallel. It returns when all awaitable objects are completed (including successful returns or exceptions). `gather()` has two overloads, accepting a variable number of awaitable objects or an iterable object containing awaitable objects.

The return value of `gather()` is `std::tuple<corio::Result<Ts>...>` or `std::vector<corio::Result<T>>`, where `Ts...` and `T` are the return values of the awaitable objects after applying `co_await`.

> [!NOTE]
> `corio::Result<T>` is a container similar to Rust's `Result`, which may contain a value of type `T` or an `exception`. You can check if the Result contains an exception by using `result.exception() != nullptr`. If there is no exception, `result.result()` returns a reference to the value stored in the Result (if `T = void`, there is no return value); if there is an exception, `result.result()` throws the exception.


```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();
corio::Lazy<int> h();

// std::tuple<Result<int>, Result<void>>
auto [r1, r2] = co_await corio::gather(
    co_await corio::spawn(f()),
    co_await corio::spawn(g()));


std::vector<corio::Task<int>> tasks;
tasks.push_back(co_await corio::spawn(f()));
tasks.push_back(co_await corio::spawn(h()));

// std::vector<Result<int>>
auto r3 = co_await corio::gather(tasks);
```

Additionally, for the variadic version of `gather()`, Corio provides an overload for the `&` operator.

```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();

using corio::awaitable_operators::operator&;

auto [r1, r2] = co_await (corio::spawn(f()) & corio::spawn(g()));
```

#### try_gather

Similar to `gather()`, `corio::try_gather()` can also wait for multiple awaitable objects in parallel and includes two overloads. The difference is that when one of the awaitable objects throws an exception first, `try_gather()` will return early. If it returns early, the remaining unfinished `co_await` operations will be canceled. The return type of `try_gather()` is `std::tuple<Ts...>` or `std::vector<T>`. If `T = void`, `T` is replaced with `std::monostate`.

> [!NOTE]
> If the awaitable objects are moved into `try_gather` using move semantics, not only will the `co_await` operations be canceled, but the awaitable objects themselves will also be destructed. For `Task<T>`, be aware of the difference between passing `task` and `std::move(task)`.


```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();
corio::Lazy<int> h();

// int, std::monostate
auto [r1, r2] = co_await corio::try_gather(
    co_await corio::spawn(f()),
    co_await corio::spawn(g()));


std::vector<corio::Task<int>> tasks;
tasks.push_back(co_await corio::spawn(f()));
tasks.push_back(co_await corio::spawn(h()));

// std::vector<int>
auto r3 = co_await corio::gather(tasks);
```

For the variadic version of `try_gather()`, Corio provides an overload for the `&&` operator.


```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();

using corio::awaitable_operators::operator&&;

auto [r1, r2] = co_await (corio::spawn(f()) && corio::spawn(g()));
```

#### select

`corio::select()` waits for multiple awaitable objects in parallel and returns when the first awaitable object completes (either successfully or with an exception), canceling the remaining unfinished `co_await` operations. `select()` also includes two overloads. The return type of `select()` is `std::variant<Ts...>` or `std::pair<std::size_t, T>`. When `T = void`, `T` is replaced with `std::monostate`.

> [!NOTE]
> When the return type is `std::variant<Ts...>`, you can use `var.index()` to determine which awaitable object completed first.
> When the return type is `std::pair<std::size_t, T>`, `std::size_t` indicates which element in the iterable container completed first.

```cpp
corio::Lazy<int> f();
corio::Lazy<void> g();
corio::Lazy<void> h();

// std::variant<std::monostate, int, std::monostate>
auto r = co_await corio::select(
    corio::this_coro::sleep_for(1ms),
    co_await corio::spawn(f()),
    co_await corio::spawn(g()),
);
if (r.index() == 0) {
    // timeout
} else {
    // not timeout
}

std::vector<corio::Task<void>> tasks;
tasks.push_back(co_await corio::spawn(g()));
tasks.push_back(co_await corio::spawn(h()));
// std::pair<std::size_t, std::monostate>
auto [index, _] = co_await corio::select(tasks);
```

> [!NOTE]
> Similar to `try_gather()`, also be aware of the difference between `task` and `std::move(task)`.

For the variadic version of `select()`, Corio provides an overload for the `||` operator.

```cpp
using corio::awaitable_operators::operator||;

corio::Lazy<int> f();
corio::Lazy<void> g();

auto r = co_await (
    corio::this_coro::sleep_for(1ms)
    || corio::spawn(f())
    || corio::spawn(g())
);
```

### Integration with Asio

Asio provides a rich set of asynchronous IO interfaces. Corio provides the completion token `corio::use_corio` to adapt to Asio.

Take `asio::steady_timer` as an example. The classic callback-based asynchronous code is as follows:


```cpp
asio::steady_timer timer(io_context, 500us);
timer.async_wait([]() {
    // do something...
});
// ...
io_context.run();
// ...
```

In Corio, we use `use_corio` to make async_wait return an awaitable object `corio::Operation<...>`.


```cpp
corio::Lazy<void> f() {
    auto ex = co_await corio::this_coro::executor;
    asio::steady_timer timer(ex, 500us);
    auto op = timer.async_wait(corio::use_corio);
    co_await op;
    // do something...
}
```

We can also use `use_corio_t::as_default_on_t` or `use_corio.as_default_on` to make `corio::use_corio` the default completion token for asynchronous operations.


```cpp
namespace corio {
    using steady_timer = use_corio_t::as_default_on_t<asio::steady_timer>;
}

corio::Lazy<void> f() {
    auto ex = co_await corio::this_coro::executor;
    // corio:: instead of asio::
    corio::steady_timer timer(ex, 500us);
    co_await timer.async_wait();
    // do something...
}

corio::Lazy<void> g() {
    auto ex = co_await corio::this_coro::executor;
    auto timer = corio::use_corio.as_default_on(asio::steady_timer(ex, 500us));
    co_await timer.async_wait();
    // do something...
}
```
