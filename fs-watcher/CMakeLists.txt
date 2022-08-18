set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

add_executable(fs_watcher fs_watcher.cpp)
target_link_libraries(fs_watcher fs_utils)
