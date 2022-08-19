set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

add_executable(fs_watcher
  fs_watcher.h
  fs_watcher.cpp
  file_loader.h
  file_loader.cpp
  main.cpp)
target_link_libraries(fs_watcher PRIVATE
  #fs_utils
  fmt::fmt)
target_include_directories(fs_watcher PUBLIC
  ${CMAKE_SOURCE_DIR}/include)


# add_executable(fs_watcher_scrap
#   fs_watcher_scrap.cpp)
# target_link_libraries(fs_watcher_scrap PRIVATE
#   #fs_utils
#   fmt::fmt)
