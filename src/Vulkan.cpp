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
#include "Log.hpp"

#include <fmt/color.h>

#include <stdexcept>
#include <vector>

namespace
{

using namespace Vulkan;

inline void throw_if(bool b, std::string_view msg)
{
  if (b) throw std::runtime_error(msg.data());
}

VkApplicationInfo to_vk_app_info(const ApplicationInfo& info)
{
  return VkApplicationInfo
  {
    .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext              = nullptr,
    .pApplicationName   = info.app_name.data(),
    .applicationVersion = info.app_version,
    .pEngineName        = info.engine_name.data(),
    .engineVersion      = info.engine_version,
    .apiVersion         = info.vulkan_version,
  };
}

void check_create_info(const VulkanCreateInfo& info)
{
  throw_if(info.width <= 0, "width of window is invalid value!");
  throw_if(info.height <= 0, "height of window is invalid value!");
  throw_if(info.title.empty(), "title not specified!");
}

auto get_supported_instance_layers()
{
  uint32_t count;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
  return layers;
}

void print_supported_instance_layers()
{
  auto layers = get_supported_instance_layers();
  fmt::print(fg(fmt::color::green), "available instance layers:\n");
  for (const auto& layer : layers)
    fmt::print(fg(fmt::color::green), "  {}\n", layer.layerName);
  fmt::println("");
}

void check_layers_support(const std::vector<std::string_view>& layers)
{
  auto supported_layers = get_supported_instance_layers();

  for (auto layer : layers)
  {
    auto it = std::find_if(supported_layers.begin(), supported_layers.end(),
                           [layer](const auto& supported_layer)
                           {
                             return supported_layer.layerName == layer;
                           });
    throw_if(it == supported_layers.end(), fmt::format("unsupported layer: {}", layer));
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
  VkDebugUtilsMessageTypeFlagsEXT             message_type,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
  void*                                       user_data)
{
  Log::info(callback_data->pMessage);
  return VK_FALSE;
}
  
auto get_debug_messenger_create_info()
{
  return VkDebugUtilsMessengerCreateInfoEXT
  {
    .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debug_messenger_callback,
  };
}

std::vector<std::string_view> get_extensions()
{
  std::vector<std::string_view> extensions =
  {
    // VK_EXT_swapchain_maintenance_1 extension need these
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
  };

  // glfw extensions
  uint32_t count;
  auto glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  extensions.insert(extensions.end(), glfw_extensions, glfw_extensions + count);

  // debug messenger extension
#ifndef NDEBUG
  extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  return extensions;
}

std::vector<VkExtensionProperties> get_supported_extensions()
{
  uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
  return extensions;
}

void print_supported_extensions()
{
  auto extensions = get_supported_extensions();
  fmt::print(fg(fmt::color::green), "available extensions:\n");
  for (const auto& extension : extensions)
    fmt::print(fg(fmt::color::green), "  {}\n", extension.extensionName);
  fmt::println("");
}

void check_extensions_support(std::vector<std::string_view> extensions)
{
  auto supported_extensions = get_supported_extensions();
  for (auto extension : extensions)
  {
    auto it = std::find_if(supported_extensions.begin(), supported_extensions.end(),
                           [extension] (const auto& supported_extension) {
                             return supported_extension.extensionName == extension;
                           });
    throw_if(it == supported_extensions.end(), fmt::format("unsupported extension: {}", extension));
  }
}

std::vector<const char*> get_vector_cstr(const std::vector<std::string_view>& strs)
{
  std::vector<const char*> cstrs(strs.size());
  for (uint32_t i = 0; i < strs.size(); ++i)
    cstrs[i] = strs[i].data();
  return cstrs;
}

}

namespace Vulkan
{

Vulkan::Vulkan(const VulkanCreateInfo& info)
{
  check_create_info(info);
  init_window(info.width, info.height, info.title);
  init_vulkan(info);
}

Vulkan::~Vulkan()
{
#ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT(_vulkan, _debug_messenger, nullptr);
#endif
  vkDestroyInstance(_vulkan, nullptr);

  glfwDestroyWindow(_window);
  glfwTerminate();
}

void Vulkan::init_window(uint32_t width, uint32_t height, std::string_view title)
{
  throw_if(glfwInit() == GLFW_FALSE, "failed to init GLFW!");

  // don't create OpenGL context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
  throw_if(_window == nullptr, "failed to create window!");
}

void Vulkan::init_vulkan(const VulkanCreateInfo& info)
{
  create_vulkan_instance(info);
#ifndef NDEBUG
  create_debug_messenger();
#endif
}

void Vulkan::create_vulkan_instance(const VulkanCreateInfo& info)
{
  // debug messenger
  auto debug_messenger_info = get_debug_messenger_create_info();

  // app info
  auto app_info = info.app_info.transform(to_vk_app_info);

  // layers
#ifndef NDEBUG
  auto validation_layer = "VK_LAYER_KHRONOS_validation";
  print_supported_instance_layers();
  check_layers_support({ validation_layer });
#endif

  // extensions
  auto extensions = get_extensions();
#ifndef NDEBUG
  print_supported_extensions();
#endif
  check_extensions_support(extensions);
  auto extensions_c = get_vector_cstr(extensions);

  // create instance
  VkInstanceCreateInfo create_info =
  {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = &debug_messenger_info,
    .flags = 0,
    .pApplicationInfo = app_info ? &app_info.value() : nullptr, 
#ifndef NDEBUG
    .enabledLayerCount   = 1,
    .ppEnabledLayerNames = &validation_layer,
#endif
    .enabledExtensionCount   = (uint32_t)extensions_c.size(),
    .ppEnabledExtensionNames = extensions_c.data(),
  };
  throw_if(vkCreateInstance(&create_info, nullptr, &_vulkan) != VK_SUCCESS,
           "failed to create vulkan instance!");
}

void Vulkan::create_debug_messenger()
{
  auto info = get_debug_messenger_create_info();
  throw_if(vkCreateDebugUtilsMessengerEXT(_vulkan, &info, nullptr, &_debug_messenger) != VK_SUCCESS,
          "failed to create debug utils messenger extension");
}

}
