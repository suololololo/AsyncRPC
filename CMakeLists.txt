cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c11 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

project(RPC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_VERBOSE_MAKEFILE ON)

set (LIB_SRC 
    src/fiber.cc
    src/scheduler.cc
    src/thread.cc
    src/utils.cc
    src/log.cc
    src/byte_array.cc
    src/address.cc
)
set(LIB_LIB pthread)

set(CMAKE_BUILD_TYPE "Debug")
# file(GLOB_RECURSE SRC_LIST ${PROJECT_SOURCE_DIR}/src/*.cc )
add_library(util ${LIB_SRC})
target_include_directories(util PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(util ${LIB_LIB})

add_executable(test_fiber ${PROJECT_SOURCE_DIR}/test/test_fiber.cc)
target_include_directories(test_fiber PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_fiber PUBLIC util)


add_executable(test_scheduler ${PROJECT_SOURCE_DIR}/test/test_scheduler.cc)
target_include_directories(test_scheduler PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_scheduler PUBLIC util)

add_executable(test_log ${PROJECT_SOURCE_DIR}/test/test_log.cc)
target_include_directories(test_log PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_log PUBLIC util)

add_executable(test_byte_array ${PROJECT_SOURCE_DIR}/test/test_byte_array.cc)
target_include_directories(test_byte_array PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_byte_array PUBLIC util)


add_executable(test_serializer ${PROJECT_SOURCE_DIR}/test/rpc/test_serializer.cc)
target_include_directories(test_serializer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_serializer PUBLIC util)


add_executable(test_address ${PROJECT_SOURCE_DIR}/test/test_address.cc)
target_include_directories(test_address PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_address PUBLIC util)