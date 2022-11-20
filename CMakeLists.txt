cmake_minimum_required(VERSION 3.10)

project(RPC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)




file(GLOB_RECURSE SRC_LIST ${PROJECT_SOURCE_DIR}/src/*.cc )
add_library(util ${SRC_LIST})
target_include_directories(util PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(test_fiber ${PROJECT_SOURCE_DIR}/test/test_fiber.cc)
target_include_directories(test_fiber PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_fiber PUBLIC util)
# target_link_libraries