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

#include <glm/glm.hpp>
#include <fmt/color.h>

#include <stdexcept>
#include <map>
#include <ranges>
#include <set>
#include <fstream>

namespace
{

using namespace Vulkan;

uint32_t Vulkan_Version = -1;

inline auto throw_if(bool b, std::string_view msg)
{
  if (b) throw std::runtime_error(msg.data());
}

auto to_vk_app_info(const ApplicationInfo& info)
{
  Vulkan_Version = info.app_version;
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
  throw_if(!info.app_info.has_value() && info.app_info->app_version == -1, "vulkan api version not specified!");
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

auto get_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
  auto it = std::find_if(formats.begin(), formats.end(),
                         [](const auto& format)
                         {
                           return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                                  format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                         });
  if (it != formats.end())
    return *it;
  return formats[0];
}

auto get_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
  auto it = std::find_if(present_modes.begin(), present_modes.end(),
                         [](const auto& mode)
                         {
                           return mode == VK_PRESENT_MODE_MAILBOX_KHR;
                         });
  if (it != present_modes.end())
    return *it;
  return VK_PRESENT_MODE_FIFO_KHR;
}

auto get_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    return capabilities.currentExtent;

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D actual_extent
  {
    (uint32_t)width,
    (uint32_t)height,
  };

  actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actual_extent;
}

auto get_file_data(std::string_view filename)
{
  std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
  throw_if(!file.is_open(), fmt::format("failed to open {}", filename));

  size_t file_size = (size_t)file.tellg();
  std::vector<char> buffer(file_size);
  
  file.seekg(0);
  file.read(buffer.data(), file_size);

  file.close();
  return buffer;
}

struct Shader
{
  VkShaderModule shader;

  Shader(VkDevice device, std::string_view filename)
    : _device(device)
  {
    auto data = get_file_data(filename);
    VkShaderModuleCreateInfo info
    {
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = data.size(),
      .pCode    = reinterpret_cast<const uint32_t*>(data.data()),
    };
    throw_if(vkCreateShaderModule(device, &info, nullptr, &shader) != VK_SUCCESS,
             fmt::format("failed to create shader from {}", filename));
  }

  ~Shader()
  {
    vkDestroyShaderModule(_device, shader, nullptr);
  }

private:
  VkDevice _device;
};

struct Vertex
{
  glm::vec2 position;
  glm::vec3 color;

  static constexpr auto get_attribute_descriptions() -> std::array<VkVertexInputAttributeDescription, 2>
  {
    return
    {
      VkVertexInputAttributeDescription
      {
        .location = 0,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32_SFLOAT,
        .offset   = offsetof(Vertex, position),
      },
      VkVertexInputAttributeDescription
      {
        .location = 1,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = offsetof(Vertex, color),
      },
    };
  }

  static constexpr auto get_binding_description()
  {
    return VkVertexInputBindingDescription
    {
      .binding   = 0,
      .stride    = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
  }

};

const std::vector<Vertex> Vertices =
{
  { { -.5f, -.5f }, { 1.f, 0.f, 0.f } },
  { {  .5f, -.5f }, { 0.f, 1.f, 0.f } },
  { {  .5f,  .5f }, { 0.f, 0.f, 1.f } },
  { { -.5f,  .5f }, { 1.f, 1.f, 1.f } },
};

const std::vector<uint16_t> Indices =
{
  0, 1, 2,
  0, 2, 3,
};

struct UniformBufferObject
{
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

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
  vmaDestroyBuffer(_vma_allocator, _vertex_buffer, _vertex_buffer_allocation);
  // TODO: use sub-allocation
  // vmaDestroyBuffer(_vma_allocator, _buffer, _vma_allocation);

  vkDestroyCommandPool(_device, _command_pool, nullptr);

  for (auto framebuffer : _swapchain_framebuffers)
    vkDestroyFramebuffer(_device, framebuffer, nullptr);

  vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
  vkDestroyPipeline(_device, _pipeline, nullptr);

  vkDestroyDescriptorSetLayout(_device, _descriptor_set_layout, nullptr);

  vkDestroyRenderPass(_device, _render_pass, nullptr);

  for (auto view : _swapchain_image_views)
    vkDestroyImageView(_device, view, nullptr);

  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  vmaDestroyAllocator(_vma_allocator);
  vkDestroyDevice(_device, nullptr);

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
  create_logical_device();
  create_swapchain();
  create_image_views();
  create_render_pass();
  create_destriptor_set_layout();
  create_pipeline();
  create_framebuffer();
  create_command_pool();
  create_command_buffers();
  create_buffers();
}

void Vulkan::create_vulkan_instance(const VulkanCreateInfo& info)
{
  // debug messenger
#ifndef NDEBUG
  auto debug_messenger_info = get_debug_messenger_create_info();
#endif

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
#ifndef NDEBUG
    .pNext = &debug_messenger_info,
#endif
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

void Vulkan::create_logical_device()
{
  auto queue_families = get_queue_family_indices(_physical_device, _surface);
  // if graphic and present family are same index, indices will be one
  std::set<uint32_t> indices
  {
    queue_families.graphics_family.value(),
    queue_families.present_family.value(),
  };
  float priority = 1.0f;

  // queue infos
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (auto index : indices)
    queue_infos.emplace_back(VkDeviceQueueCreateInfo
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = index,
      .queueCount = 1,
      .pQueuePriorities = &priority,
    });

  // TODO: currently empty
  VkPhysicalDeviceFeatures features{};

  // device info 
  VkDeviceCreateInfo create_info
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = (uint32_t)queue_infos.size(),
    .pQueueCreateInfos = queue_infos.data(),
    .enabledExtensionCount = (uint32_t)Device_Extensions.size(),
    .ppEnabledExtensionNames = Device_Extensions.data(),
    .pEnabledFeatures = &features,
  };

  // create logical device
  throw_if(vkCreateDevice(_physical_device, &create_info, nullptr, &_device) != VK_SUCCESS,
           "failed to create logical device");

  // get queues
  vkGetDeviceQueue(_device, queue_families.graphics_family.value(), 0, &_graphics_queue);
  vkGetDeviceQueue(_device, queue_families.present_family.value(), 0, &_graphics_queue);

  // create VmaAllocator
  VmaAllocatorCreateInfo alloc_info
  {
    .flags            = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
    .physicalDevice   = _physical_device,
    .device           = _device,
    .instance         = _vulkan,
    .vulkanApiVersion = Vulkan_Version,
  };
  throw_if(vmaCreateAllocator(&alloc_info, &_vma_allocator) != VK_SUCCESS,
           "failed to create Vulkan Memory Allocator");
}

void Vulkan::create_swapchain()
{
  auto details = get_swapchain_details(_physical_device, _surface);
  auto surface_format = get_surface_format(details.formats);
  auto present_mode = get_present_mode(details.present_modes);
  auto extent = get_swap_extent(details.capabilities, _window);
  uint32_t image_count = details.capabilities.minImageCount + 1;
  if (details.capabilities.maxImageCount > 0 &&
      image_count > details.capabilities.maxImageCount)
    image_count = details.capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR create_info
  {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = _surface,
    .minImageCount = image_count,
    .imageFormat = surface_format.format,
    .imageColorSpace = surface_format.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = details.capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    .clipped = VK_TRUE,
  };

  auto queue_families = get_queue_family_indices(_physical_device, _surface);
  uint32_t indices[]
  {
    queue_families.graphics_family.value(),
    queue_families.present_family.value(),
  };

  if (queue_families.graphics_family != queue_families.present_family)
  {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = indices;
  }
  else
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  throw_if(vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain) != VK_SUCCESS,
      "failed to create swapchain");

  vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr);
  _swapchain_images.resize(image_count);
  vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data());
  _swapchain_image_format = surface_format.format;
  _swapchain_image_extent = extent;
}

void Vulkan::create_image_views()
{
  _swapchain_image_views.resize(_swapchain_images.size());
  for (uint32_t i = 0; i < _swapchain_images.size(); ++i)
  {
    VkImageViewCreateInfo info
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = _swapchain_images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = _swapchain_image_format,
      .components =
      {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange =
      {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };
    throw_if(vkCreateImageView(_device, &info, nullptr, &_swapchain_image_views[i]) != VK_SUCCESS,
            "failed to create image view");
  }
}

void Vulkan::create_render_pass()
{
  VkAttachmentDescription color_attachment
  {
    .format         = _swapchain_image_format,
    .samples        = VK_SAMPLE_COUNT_1_BIT,
    .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference attach_reference
  {
    .attachment = 0,
    .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass
  {
    .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments    = &attach_reference,
  };

  VkSubpassDependency dependency 
  {
    .srcSubpass    = VK_SUBPASS_EXTERNAL,
    .dstSubpass    = 0,
    .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo create_info
  {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments    = &color_attachment,
    .subpassCount    = 1,
    .pSubpasses      = &subpass,
    .dependencyCount = 1,
    .pDependencies   = &dependency,
  };

  throw_if(vkCreateRenderPass(_device, &create_info, nullptr, &_render_pass) != VK_SUCCESS,
           "failed to create render pass");
}

void Vulkan::create_destriptor_set_layout()
{
  VkDescriptorSetLayoutBinding layout
  {
    .binding         = 0,
    .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
  };

  VkDescriptorSetLayoutCreateInfo info
  {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings    = &layout,
  };

  throw_if(vkCreateDescriptorSetLayout(_device, &info, nullptr, &_descriptor_set_layout) != VK_SUCCESS,
           "failed to create descriptor set layout");
}

void Vulkan::create_pipeline()
{
  // shader stages
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

  Shader vertex_shader(_device, "shader/vertex.spv");
  Shader fragment_shader(_device, "shader/fragment.spv");

  VkPipelineShaderStageCreateInfo shader_info
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vertex_shader.shader,
    .pName = "main",
  };
  shader_stages.emplace_back(shader_info);

  shader_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_info.module = fragment_shader.shader;
  shader_stages.emplace_back(shader_info);

  // vertex input info
  auto binding_desc = Vertex::get_binding_description();
  auto attribute_descs = Vertex::get_attribute_descriptions();
  VkPipelineVertexInputStateCreateInfo vertex_input_info
  {
    .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = 1,
    .pVertexBindingDescriptions      = &binding_desc,
    .vertexAttributeDescriptionCount = (uint32_t)attribute_descs.size(),
    .pVertexAttributeDescriptions    = attribute_descs.data(),
  };

  // input assembly
  VkPipelineInputAssemblyStateCreateInfo input_assembly
  {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  // viewport state
  VkPipelineViewportStateCreateInfo viewport_state
  {
    .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount  = 1,
  };

  // rasterization
  VkPipelineRasterizationStateCreateInfo rasterization_state
  {
    .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable        = VK_FALSE,   
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .cullMode                = VK_CULL_MODE_BACK_BIT,
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable         = VK_FALSE,
    .lineWidth               = 1.f,
  };

  // multisample
  VkPipelineMultisampleStateCreateInfo multisample_state
  {
    .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable  = VK_FALSE,
    .minSampleShading     = 1.f,
  };

  // color blend
  VkPipelineColorBlendAttachmentState color_blend_attachment
  {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                      VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT |
                      VK_COLOR_COMPONENT_A_BIT,
  };
  VkPipelineColorBlendStateCreateInfo color_blend
  {
    .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable   = VK_FALSE,
    .logicOp         = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments    = &color_blend_attachment,
  };

  // dynamic
  std::vector<VkDynamicState> dynamics
  {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic
  {
    .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = (uint32_t)dynamics.size(),
    .pDynamicStates    = dynamics.data(),
  };

  // pipeline layout
  VkPipelineLayoutCreateInfo layout_info
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts    = &_descriptor_set_layout,
  };
  throw_if(vkCreatePipelineLayout(_device, &layout_info, nullptr, &_pipeline_layout) != VK_SUCCESS,
           "failed to create pipeline layout");

  VkGraphicsPipelineCreateInfo create_info
  {
    .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount          = 2,
    .pStages             = shader_stages.data(),
    .pVertexInputState   = &vertex_input_info,
    .pInputAssemblyState = &input_assembly,
    .pViewportState      = &viewport_state,
    .pRasterizationState = &rasterization_state,
    .pMultisampleState   = &multisample_state,
    .pColorBlendState    = &color_blend,
    .pDynamicState       = &dynamic,
    .layout              = _pipeline_layout,
    .renderPass          = _render_pass,
    .subpass             = 0,
    .basePipelineHandle  = VK_NULL_HANDLE,
    .basePipelineIndex   = -1,
  };

  throw_if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &create_info, nullptr, &_pipeline) != VK_SUCCESS,
           "failed to create pipeline");
}

void Vulkan::create_framebuffer()
{
  _swapchain_framebuffers.resize(_swapchain_images.size());
  for (uint32_t i = 0; i < _swapchain_images.size(); ++i)
  {
    VkFramebufferCreateInfo info
    {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = _render_pass,
      .attachmentCount = 1,
      .pAttachments    = &_swapchain_image_views[i],
      .width           = _swapchain_image_extent.width,
      .height          = _swapchain_image_extent.height,
      .layers          = 1,
    };
    throw_if(vkCreateFramebuffer(_device, &info, nullptr, &_swapchain_framebuffers[i]) != VK_SUCCESS,
            "failed to create framebuffer");
  }
}

void Vulkan::create_command_pool()
{
  auto queue_families = get_queue_family_indices(_physical_device, _surface);
  VkCommandPoolCreateInfo info
  {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queue_families.graphics_family.value(),
  };
  throw_if(vkCreateCommandPool(_device, &info, nullptr, &_command_pool) != VK_SUCCESS,
           "failed to create command pool");
}

void Vulkan::create_command_buffers()
{
  VkCommandBufferAllocateInfo info
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = _command_pool,
    .level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = Max_Frame_Number,
  };
  throw_if(vkAllocateCommandBuffers(_device, &info, _command_buffers.data()) != VK_SUCCESS,
           "failed to create command buffers");
}

void Vulkan::create_buffers()
{
  // TODO: vertex, index and uniform use single buffer(sub-allocation)

  uint32_t size = sizeof(Vertices[0]) * Vertices.size();

  // create stage buffer
  VkBufferCreateInfo buffer_create_info
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size  = size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
             // sizeof(Indices[0]) * Indices.size()   +
             // sizeof(UniformBufferObject) * Max_Frame_Number,
    // .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  |
    //          VK_BUFFER_USAGE_INDEX_BUFFER_BIT   |
    //          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    //          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  VmaAllocationCreateInfo alloc_create_info
  {
    .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    .usage = VMA_MEMORY_USAGE_AUTO,
  };

  VkBuffer          stage_buffer;
  VmaAllocation     alloc;
  throw_if(vmaCreateBuffer(_vma_allocator, &buffer_create_info, &alloc_create_info, &stage_buffer, &alloc, nullptr) != VK_SUCCESS,
           "failed to create buffer");

  // copy data to stage buffer
  throw_if(vmaCopyMemoryToAllocation(_vma_allocator, Vertices.data(), alloc, 0, size) != VK_SUCCESS,
           "failed to copy data to stage buffer");

  // create vertex buffer
  buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  alloc_create_info.flags  = 0;
  throw_if(vmaCreateBuffer(_vma_allocator, &buffer_create_info, &alloc_create_info, &_vertex_buffer, &_vertex_buffer_allocation, nullptr) != VK_SUCCESS,
           "failed to create buffer");
  // copy stage buffer data to vertex buffer
  copy_buffer(stage_buffer, _vertex_buffer, size);

  vmaDestroyBuffer(_vma_allocator, stage_buffer, alloc);
}

}
