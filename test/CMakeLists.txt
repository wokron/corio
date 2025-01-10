# file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")
set(sources main.cpp test_result.cpp test_assert.cpp test_serial_runner.cpp test_lazy_new.cpp test_generator_new.cpp)
add_executable(test ${sources})
target_link_libraries(test PRIVATE doctest corio)
