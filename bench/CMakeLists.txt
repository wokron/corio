add_library(marker INTERFACE)
target_include_directories(marker INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/marker)

if(CORIO_BUILD_TESTS)
    add_executable(test_marker marker/test_marker.cpp)
    target_link_libraries(test_marker marker doctest)

    add_test(NAME test_marker COMMAND test_marker)
endif()

set(REQUIRED_LIBRARIES corio asio marker)

add_executable(post post.cpp)
target_link_libraries(post PRIVATE ${REQUIRED_LIBRARIES})

add_executable(parallel parallel.cpp)
target_link_libraries(parallel PRIVATE ${REQUIRED_LIBRARIES})

add_executable(immediate immediate.cpp)
target_link_libraries(immediate PRIVATE ${REQUIRED_LIBRARIES})

add_executable(spawn spawn.cpp)
target_link_libraries(spawn PRIVATE ${REQUIRED_LIBRARIES})

add_executable(sort sort.cpp)
target_link_libraries(sort PRIVATE ${REQUIRED_LIBRARIES})