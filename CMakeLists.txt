cmake_minimum_required(VERSION 3.30)

project(Vulkan-Triangle)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# defines
add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DSPDLOG_HEADER_ONLY)

# lib
# spdlog
find_package(spdlog CONFIG REQUIRED)
# vma
include(FetchContent)
FetchContent_Declare(
  VMA
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG       master 
)
FetchContent_MakeAvailable(VMA)

file(GLOB_RECURSE SOURCE src/*.cpp)
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

add_executable(test test.cpp ${SOURCE})

target_include_directories(test PRIVATE include)

target_link_libraries(triangle PRIVATE ${LIBS})
target_link_libraries(test PRIVATE ${LIBS})
target_link_libraries(test PRIVATE GPUOpen::VulkanMemoryAllocator)

# doc
find_package(Doxygen REQUIRED)

set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_AUTOLINK_SUPPORT NO)

doxygen_add_docs(doc ALL
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
