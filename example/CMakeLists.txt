add_executable(merge_sort merge_sort.cpp)
target_link_libraries(merge_sort PRIVATE corio)

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE corio asio)

add_executable(http_ok_server http_ok_server.cpp)
target_link_libraries(http_ok_server PRIVATE corio asio)
