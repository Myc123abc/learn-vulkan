cmake_minimum_required(VERSION 3.30)

project(Vulkan-Triangle)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DSPDLOG_HEADER_ONLY)

find_package(spdlog REQUIRED)

set(LIBS
  glfw
  vulkan
  dl
  pthread
  X11
  Xxf86vm
  Xrandr
  Xi
)

add_executable(triangle main.cpp)

add_executable(test test.cpp)

target_include_directories(test PRIVATE include)

target_link_libraries(triangle PRIVATE ${LIBS})
target_link_libraries(test PRIVATE ${LIBS})
