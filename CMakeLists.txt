cmake_minimum_required(VERSION 3.19)
project(SwiftySyncServer)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(SERVER)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_subdirectory(SwiftySyncAuthorization)
add_subdirectory(SwiftySyncCommon)
add_subdirectory(SwiftySyncStorage)

find_path(WS_INCLUDE_DIR uv.h)
find_library(UV_LIBRARY libuv)
find_library(US_LIBRARY uSockets)
find_library(ZLIB_LIBRARY z)

include_directories(timercpp)

add_library(SwiftySyncServer SwiftySyncServer.cpp SwiftySyncServer.h)
target_link_libraries(SwiftySyncServer ${UV_LIBRARY} ${US_LIBRARY} ${ZLIB_LIBRARIES} Codable SwiftySyncAuthorization SwiftySyncCommon SwiftySyncStorage ${OPENSSL_LIBRARIES})
target_include_directories(SwiftySyncServer PUBLIC ${OPENSSL_INCLUDE_DIR} PUBLIC ${ZLIB_INCLUDE_DIRS} PUBLIC "/" PUBLIC SwiftySyncCommon/CodablePP PUBLIC SwiftySyncAuthorization PUBLIC SwiftySyncCommon PUBLIC SwiftySyncStorage PUBLIC ${WS_INCLUDE_DIR})

add_executable(test_server main.cpp)
target_link_libraries(test_server SwiftySyncServer)