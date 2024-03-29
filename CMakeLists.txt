cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c11 -Wall -Wno-deprecated  -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

project(RPC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_VERBOSE_MAKEFILE ON)

set (LIB_SRC 
    src/address.cc
    src/byte_array.cc
    src/fd_manager.cc 
    src/fiber.cc
    src/hook.cc
    src/io_manager.cc
    src/log.cc
    src/mutex.cc
    src/scheduler.cc
    src/socket.cc
    src/socket_stream.cc
    src/stream.cc
    src/tcp_server.cc
    src/thread.cc
    src/timer.cc
    src/utils.cc
    src/rpc/rpc_client.cc
    src/rpc/rpc_connection_pool.cc
    src/rpc/rpc_server.cc
    src/rpc/rpc_service_registry.cc
    src/rpc/rpc_session.cc
    
)
set(LIB_LIB 
    pthread
    dl
    )

set(CMAKE_BUILD_TYPE "Debug")
# set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib")
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/install)
file(GLOB HFILE_LIST ${PROJECT_SOURCE_DIR}/include/*.h )
file(GLOB HFILE_LIST2 ${PROJECT_SOURCE_DIR}/include/rpc/*.h )
add_library(util SHARED ${LIB_SRC})
target_include_directories(util PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(util ${LIB_LIB})

install(TARGETS util LIBRARY DESTINATION lib)
install(FILES ${HFILE_LIST} DESTINATION include)
install(FILES ${HFILE_LIST2} DESTINATION include/rpc)

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

add_executable(test_iomanager ${PROJECT_SOURCE_DIR}/test/test_iomanager.cc)
target_include_directories(test_iomanager PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_iomanager PUBLIC util)

add_executable(test_hook ${PROJECT_SOURCE_DIR}/test/test_hook.cc)
target_include_directories(test_hook PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_hook PUBLIC util)

add_executable(test_socket ${PROJECT_SOURCE_DIR}/test/test_socket.cc)
target_include_directories(test_socket PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_socket PUBLIC util)

add_executable(test_tcp_server ${PROJECT_SOURCE_DIR}/test/test_tcp_server.cc)
target_include_directories(test_tcp_server PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_tcp_server PUBLIC util)


add_executable(test_echo_server ${PROJECT_SOURCE_DIR}/test/test_echo_server.cc)
target_include_directories(test_echo_server PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_echo_server PUBLIC util)


add_executable(test_rpc_server ${PROJECT_SOURCE_DIR}/test/rpc/test_rpc_server.cc)
target_include_directories(test_rpc_server PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_rpc_server PUBLIC util)



add_executable(test_rpc_service_registry ${PROJECT_SOURCE_DIR}/test/rpc/test_rpc_service_registry.cc)
target_include_directories(test_rpc_service_registry PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_rpc_service_registry PUBLIC util)

add_executable(test_coMutex ${PROJECT_SOURCE_DIR}/test/test_coMutex.cc)
target_include_directories(test_coMutex PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_coMutex PUBLIC util)

add_executable(test_rpc_client ${PROJECT_SOURCE_DIR}/test/rpc/test_rpc_client.cc)
target_include_directories(test_rpc_client PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_rpc_client PUBLIC util)

add_executable(test_channel ${PROJECT_SOURCE_DIR}/test/test_channel.cc)
target_include_directories(test_channel PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_channel PUBLIC util)

add_executable(test_bug ${PROJECT_SOURCE_DIR}/test/test_bug.cc)
target_include_directories(test_bug PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_bug PUBLIC util)


add_executable(test_rpc_connection_pool ${PROJECT_SOURCE_DIR}/test/rpc/test_rpc_connection_pool.cc)
target_include_directories(test_rpc_connection_pool PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_rpc_connection_pool PUBLIC util)