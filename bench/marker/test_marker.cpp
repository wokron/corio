#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <chrono>
#include <doctest/doctest.h>
#include <marker.hpp>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("test since") {
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(500us);
    auto duration = marker::since<std::chrono::microseconds>(start);
    CHECK(duration >= 500us);
}

TEST_CASE("test timer") {
    marker::Timer timer;
    timer.tick();
    std::this_thread::sleep_for(500us);
    timer.tock();
    auto duration = timer.duration<std::chrono::microseconds>();
    CHECK(duration >= 500us);
}

TEST_CASE("test scope timer") {
    auto duration = std::make_shared<std::chrono::microseconds>();
        
    {
        marker::ScopeTimer timer(duration);
        std::this_thread::sleep_for(500us);
    }
    CHECK(*duration >= 500us);
}

TEST_CASE("test measured") {
    auto fn = [](int x) {
        std::this_thread::sleep_for(500us);
        return x + 42;
    };
    auto measured_fn = marker::measured<std::chrono::microseconds>(fn);
    auto [r, duration] = measured_fn(1);
    CHECK(r == 43);
    CHECK(duration >= 500us);
}