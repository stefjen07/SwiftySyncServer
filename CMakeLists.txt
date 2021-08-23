cmake_minimum_required(VERSION 3.19)
project(SwiftySyncServer)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(SERVER)

add_subdirectory(SwiftySyncAuthorization)
add_subdirectory(SwiftySyncCommon)
add_subdirectory(SwiftySyncStorage)

find_path(WS_INCLUDE_DIR uv.h)
find_library(UV_LIBRARY libuv)
find_library(US_LIBRARY uSockets)
find_library(ZLIB_LIBRARY z)

set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl-cmake/include)
set(OPENSSL_LIBRARIES ssl crypto)

include_directories(include)
include_directories(timercpp)

add_library(SwiftySyncServer src/SwiftySyncServer.cpp include/SwiftySyncServer.h)
target_link_libraries(SwiftySyncServer ${UV_LIBRARY} ${US_LIBRARY} ${ZLIB_LIBRARY} Codable SwiftySyncAuthorization SwiftySyncCommon SwiftySyncStorage ${OPENSSL_LIBRARIES})
target_include_directories(SwiftySyncServer PUBLIC ${WS_INCLUDE_DIR} PUBLIC ${OPENSSL_INCLUDE_DIR} PUBLIC SwiftySyncCommon/CodablePP/include PUBLIC SwiftySyncAuthorization/include PUBLIC SwiftySyncCommon/include PUBLIC SwiftySyncStorage/include)

add_executable(test_server test/test.cpp)
target_link_libraries(test_server SwiftySyncServer)