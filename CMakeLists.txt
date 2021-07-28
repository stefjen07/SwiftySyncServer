cmake_minimum_required(VERSION 3.19)
project(SwiftySyncServer)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(SERVER)

include_directories(/)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

add_subdirectory(SwiftySyncAuthorization)
add_subdirectory(SwiftySyncCommon)
add_subdirectory(SwiftySyncStorage)

find_path(WS_INCLUDE_DIR uv.h)
include_directories(${WS_INCLUDE_DIR})
find_library(UV_LIBRARY libuv)
find_library(US_LIBRARY uSockets)
find_library(ZLIB_LIBRARY z)

include_directories(SwiftySyncAuthorization)
include_directories(SwiftySyncCommon)
include_directories(SwiftySyncStorage)

include_directories(SwiftySyncCommon/CodablePP)
include_directories(timercpp)
include_directories(SwiftySyncAuthorization/cpp-httplib)

add_library(SwiftySyncServer SwiftySyncServer.cpp SwiftySyncServer.h)
target_link_libraries(SwiftySyncServer ${UV_LIBRARY} ${US_LIBRARY} ${ZLIB_LIBRARIES} Codable SwiftySyncAuthorization SwiftySyncCommon SwiftySyncStorage ${OPENSSL_LIBRARIES})

add_executable(test_server main.cpp)
target_link_libraries(test_server SwiftySyncServer)