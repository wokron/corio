cmake_minimum_required(VERSION 3.0)

project(corio)

set(CMAKE_CXX_STANDARD 20)

# Third party libraries
add_subdirectory(third_party/doctest)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE third_party/asio/asio/include)

add_library(corio INTERFACE)
target_include_directories(corio INTERFACE include)
target_link_libraries(corio INTERFACE asio)

add_subdirectory(test)