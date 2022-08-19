cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

#find_package(Boost REQUIRED)
find_package(fmt REQUIRED)

# add_library(fs_utils INTERFACE pathutils.h)
# target_include_directories(fs_utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(fs-watcher)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
