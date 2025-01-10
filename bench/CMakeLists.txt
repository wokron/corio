add_library(marker INTERFACE)
target_include_directories(marker INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/marker)

add_executable(test_marker marker/test_marker.cpp)
target_link_libraries(test_marker marker doctest)

set(REQUIRED_LIBRARIES corio asio marker)

add_executable(post post.cpp)
target_link_libraries(post ${REQUIRED_LIBRARIES})

add_executable(parallel parallel.cpp)
target_link_libraries(parallel ${REQUIRED_LIBRARIES})

add_executable(immediate immediate.cpp)
target_link_libraries(immediate ${REQUIRED_LIBRARIES})