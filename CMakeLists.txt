cmake_minimum_required(VERSION 3.19)
project(SwiftySyncServer)

set(CMAKE_CXX_STANDARD 20)

include_directories(/)

add_subdirectory(SwiftySyncAuthorization)
add_subdirectory(SwiftySyncCommon)
add_subdirectory(SwiftySyncStorage)

include_directories(SwiftySyncAuthorization)
include_directories(SwiftySyncCommon)
include_directories(SwiftySyncStorage)

include_directories(vcpkg_installed/x64-windows/include)
include_directories(SwiftySyncCommon/CodablePP)
include_directories(timercpp)

add_library(SwiftySyncServer SwiftySyncServer.cpp SwiftySyncServer.h)