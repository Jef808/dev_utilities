cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# find_package(Boost OPTIONAL)
find_package(fmt REQUIRED)

add_subdirectory(fs-watcher)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
