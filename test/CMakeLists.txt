file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")
add_executable(tests ${sources})
target_link_libraries(tests PRIVATE doctest corio asio)

add_test(NAME tests COMMAND tests)
