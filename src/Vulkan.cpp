/*===-- src/Vulkan.cpp --------- Vulkan -----------------------------------===*\
|*                                                                            *|
|* Copyright (c) 2025 Ma Yuncong                                              *|
|* Licensed under the MIT License.                                            *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* This header implement the Vulkan instance.                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#include "Vulkan.hpp"

#include <stdexcept>

namespace
{

using namespace Vulkan;

inline void throw_if(bool b, std::string_view msg)
{
  if (b) throw std::runtime_error(msg.data());
}

VkApplicationInfo get_VkApplicationInfo(const ApplicationInfo& info)
{
  VkApplicationInfo ret_info =
  {
    .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext              = nullptr,
    .pApplicationName   = info.app_name.data(),
    .applicationVersion = info.app_version,
    .pEngineName        = info.engine_name.data(),
    .engineVersion      = info.engine_version,
    .apiVersion         = info.vulkan_version,
  };
  return ret_info;
}

}

namespace Vulkan
{

Vulkan::Vulkan(const VulkanCreateInfo& info)
{
  init_window(info.width, info.height, info.title);
  init_vulkan(info);
}

void Vulkan::init_window(uint32_t width, uint32_t height, std::string_view title)
{
  throw_if(glfwInit() == GLFW_FALSE, "Failed to init GLFW!");

  // don't create OpenGL context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
  throw_if(_window == nullptr, "Failed to create window!");
}

void Vulkan::init_vulkan(const VulkanCreateInfo& info)
{
  create_vulkan_instance(info);
}
  
void Vulkan::create_vulkan_instance(const VulkanCreateInfo& info)
{
  VkApplicationInfo app_info;
  if (info.app_info.has_value())
    app_info = get_VkApplicationInfo(info.app_info.value());

  VkInstanceCreateInfo creata_info =
  {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .flags = 0,
    .pApplicationInfo = info.app_info.has_value() ? &app_info : nullptr,
  };
}

}
