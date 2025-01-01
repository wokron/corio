file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")
add_executable(test ${sources})
target_link_libraries(test PRIVATE doctest corio)
