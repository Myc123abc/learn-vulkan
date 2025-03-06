cmake_minimum_required(VERSION 3.30)

project(Vulkan-Triangle)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DFMT_HEADER_ONLY)

find_package(spdlog REQUIRED)

add_executable(triangle main.cpp)
add_executable(test test.cpp)

target_link_libraries(triangle PRIVATE
  spdlog::spdlog
  glfw
  vulkan
  dl
  pthread
  X11
  Xxf86vm
  Xrandr
  Xi
)

target_link_libraries(test PRIVATE
  spdlog::spdlog
  glfw
  vulkan
  dl
  pthread
  X11
  Xxf86vm
  Xrandr
  Xi
)
