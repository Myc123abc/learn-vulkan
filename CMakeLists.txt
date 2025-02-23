cmake_minimum_required(VERSION 3.30)

project(Vulkan-Triangle)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(spdlog REQUIRED)

add_executable(triangle main.cpp)

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
