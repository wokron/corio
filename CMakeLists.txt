cmake_minimum_required(VERSION 3.0)

project(corio)

set(IS_MAIN_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(IS_MAIN_PROJECT ON)
endif()

add_library(corio INTERFACE)
target_include_directories(corio INTERFACE include)

if(IS_MAIN_PROJECT)
    option(CORIO_BUILD_TESTS "Build the tests" ON)
    option(CORIO_BUILD_BENCH "Build the benchmarks" ON)
    option(CORIO_BUILD_EXAMPLES "Build the examples" ON)

    set(CMAKE_CXX_STANDARD 20)

    if (CORIO_BUILD_TESTS OR CORIO_BUILD_BENCH OR CORIO_BUILD_EXAMPLES)
        # Import asio
        add_library(asio INTERFACE)
        target_include_directories(asio INTERFACE third_party/asio/asio/include)
    endif()

    if(CORIO_BUILD_TESTS)
        # Import doctest
        add_subdirectory(third_party/doctest)
        
        enable_testing()
        add_subdirectory(test)
    endif()

    if (CORIO_BUILD_BENCH)
        add_subdirectory(bench)
    endif()

    if (CORIO_BUILD_EXAMPLES)
        add_subdirectory(example)
    endif()  
endif()
