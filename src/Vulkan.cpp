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
#include <map>
#include <ranges>

namespace
{

using namespace Vulkan;

inline auto throw_if(bool b, std::string_view msg)
{
  if (b) throw std::runtime_error(msg.data());
}

auto to_vk_app_info(const ApplicationInfo& info)
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

auto check_create_info(const VulkanCreateInfo& info)
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

auto print_supported_instance_layers()
{
  auto layers = get_supported_instance_layers();
  fmt::print(fg(fmt::color::green), "available instance layers:\n");
  for (const auto& layer : layers)
    fmt::print(fg(fmt::color::green), "  {}\n", layer.layerName);
  fmt::println("");
}

auto check_layers_support(const std::vector<std::string_view>& layers)
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

auto get_instance_extensions()
{
  std::vector<const char*> extensions =
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

auto get_supported_instance_extensions()
{
  uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
  return extensions;
}

auto print_supported_instance_extensions()
{
  auto extensions = get_supported_instance_extensions();
  fmt::print(fg(fmt::color::green), "available extensions:\n");
  for (const auto& extension : extensions)
    fmt::print(fg(fmt::color::green), "  {}\n", extension.extensionName);
  fmt::println("");
}

auto check_instance_extensions_support(std::vector<const char*> extensions)
{
  auto supported_extensions = get_supported_instance_extensions();
  for (auto extension : extensions)
  {
    auto it = std::find_if(supported_extensions.begin(), supported_extensions.end(),
                           [extension] (const auto& supported_extension) {
                             return strcmp(supported_extension.extensionName, extension) == 0;
                           });
    throw_if(it == supported_extensions.end(), fmt::format("unsupported extension: {}", extension));
  }
}

auto get_supported_physical_devices(VkInstance instance)
{
  uint32_t count;
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  throw_if(count == 0, "failed to find GPUs with vulkan support");
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance, &count, devices.data());
  return devices;
}

auto get_physical_devices_score(const std::vector<VkPhysicalDevice>& devices)
{
  std::multimap<int, VkPhysicalDevice> devices_score;

  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceFeatures features;
  for (const auto& device : devices)
  {
    int score = 0;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      score += 1000;
    score += properties.limits.maxImageDimension2D;
    if (!features.geometryShader)
      score = 0;
    devices_score.insert(std::make_pair(score, device));
  }

  return devices_score;
}

void print_supported_physical_devices(VkInstance instance)
{
  auto devices = get_supported_physical_devices(instance);
  auto devices_score = get_physical_devices_score(devices);
  fmt::print(fg(fmt::color::green),
             "available physical devices:\n"
             "  name\t\t\t\t\tscore\n");
  VkPhysicalDeviceProperties property;
  for (const auto& [score, device] : devices_score)
  {
    vkGetPhysicalDeviceProperties(device, &property);
    fmt::print(fg(fmt::color::green),
               "  {}\t{}\n", property.deviceName, score);
  }
  fmt::println("");
}

auto get_supported_queue_families(VkPhysicalDevice device)
{
  uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
  return families;
}

struct QueueFamilyIndices
{
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  auto has_all_queue_families()
  {
    return graphics_family.has_value() &&
           present_family.has_value();
  }
};

auto get_queue_family_indices(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  auto queue_families = get_supported_queue_families(device);
  std::vector<QueueFamilyIndices> all_indices;
  VkBool32 WSI_support;
  for (uint32_t i = 0; i < queue_families.size(); ++i)
  {
    QueueFamilyIndices indices;

    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indices.graphics_family = i;
    
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &WSI_support);
    if (WSI_support)
      indices.present_family = i;

    if (indices.has_all_queue_families())
      all_indices.emplace_back(indices);
  }

  throw_if(all_indices.empty(), "failed to support necessary queue families");

  // some queue features may be in a same index,
  // so less queues best performance when queue is not much
  auto it = std::find_if(all_indices.begin(), all_indices.end(),
    [](const auto& indicies)
    {
      return indicies.graphics_family.value() == indicies.present_family.value();
    });
  if (it == all_indices.end())
    return all_indices[0];
  return *it;
}

auto get_supported_device_extensions(VkPhysicalDevice device)
{
  uint32_t count;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
  return extensions;
}

auto print_supported_device_extensions(VkPhysicalDevice device)
{
  fmt::print(fg(fmt::color::green), "available device extensions:\n");
  for (const auto& extension : get_supported_device_extensions(device))
    fmt::print(fg(fmt::color::green), "  {}\n", extension.extensionName);
  fmt::println("");
}

const std::vector<const char*> Device_Extensions = 
{
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  // swapchain maintenance extension can auto recreate swapchain
  VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
};

auto check_device_extensions_support(VkPhysicalDevice device, const std::vector<const char*>& extensions)
{
  auto supported_extensions = get_supported_device_extensions(device);
  for (auto extension : extensions) 
  {
      auto it = std::find_if(supported_extensions.begin(), supported_extensions.end(),
                             [extension] (const auto& supported_extension)
                             {
                               return strcmp(extension, supported_extension.extensionName) == 0;
                             });
      if (it == supported_extensions.end())
        return false;
  }
  return true;
}

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR        capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR>   present_modes;

  auto has_empty()
  {
    return formats.empty() || present_modes.empty();
  }
};

auto get_swapchain_details(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
  details.formats.resize(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());

  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
  details.present_modes.resize(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.present_modes.data());

  return details;
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
  vkDestroySurfaceKHR(_vulkan, _surface, nullptr);

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
  create_surface();
  select_physical_device();
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
  auto extensions = get_instance_extensions();
#ifndef NDEBUG
  print_supported_instance_extensions();
#endif
  check_instance_extensions_support(extensions);

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
    .enabledExtensionCount   = (uint32_t)extensions.size(),
    .ppEnabledExtensionNames = extensions.data(),
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

void Vulkan::create_surface()
{
  throw_if(glfwCreateWindowSurface(_vulkan, _window, nullptr, &_surface) != VK_SUCCESS,
           "failed to create surface");
}

void Vulkan::select_physical_device()
{
  auto devices = get_supported_physical_devices(_vulkan);
  auto devices_score = get_physical_devices_score(devices);
  
  for (const auto& [score, device] : std::ranges::views::reverse(devices_score))
  {
    if (score > 0)
    {
      auto queue_family_indices = get_queue_family_indices(device, _surface);
      if (check_device_extensions_support(device, Device_Extensions) &&
          !get_swapchain_details(device, _surface).has_empty())
      {
        _physical_device = device;
        break;
      }
    }
  }

  throw_if(_physical_device == VK_NULL_HANDLE, "failed to find a suitable GPU");

#ifndef NDEBUG
  print_supported_physical_devices(_vulkan);
  print_supported_device_extensions(_physical_device);
#endif
}

}
