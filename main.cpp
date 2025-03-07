#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <spdlog/spdlog.h>
#include <fmt/color.h>

#include <vector>
#include <stdexcept>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <fstream>
#include <array>

// TODO: When rewrite this file, remember use XXX2 new struct or function to replace the old one.
// HACK: Allocate multi-resources like buffer in single memory allocation,
//       and store vertices and indices in single buffer.

class App
{
private:
  // Vertex Structure
  struct Vertex
  {
    glm::vec2 position;
    glm::vec3 color;

    static auto get_binding_description()
    {
      return VkVertexInputBindingDescription
      {
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }

    static auto get_attribute_descriptions()
    {
      std::array<VkVertexInputAttributeDescription, 2> descs;

      descs[0].binding  = 0;
      descs[0].location = 0;
      descs[0].format   = VK_FORMAT_R32G32_SFLOAT;
      descs[0].offset   = offsetof(Vertex, position);

      descs[1].binding  = 0;
      descs[1].location = 1;
      descs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
      descs[1].offset   = offsetof(Vertex, color);

      return descs;
    }
  };

public:
  void run()
  {
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
  }

private:
  void init_window()
  {
    if (glfwInit() == GLFW_FALSE)
      throw std::runtime_error("failed to init GLFW!");

    // don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _win = glfwCreateWindow(Window_Width, Window_Height, "Vulkan", nullptr, nullptr);
    if (_win == nullptr)
      throw std::runtime_error("failed to create window!");
  }

  void init_vulkan()
  {
    create_instance();
    set_debug_messenger();
    create_surface();
    select_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_frambuffers();
    create_command_pool();
    create_vertex_buffer();
    create_index_buffer();
    create_command_buffers();
    create_sync_objects();
  }

  void main_loop() 
  {
    while (!glfwWindowShouldClose(_win))
    {
      glfwPollEvents();
      draw_frame();
    }

    vkDeviceWaitIdle(_device);
  }

  void cleanup() 
  {
    cleanup_swapchain();

    vkDestroyBuffer(_device, _vertex_buffer, nullptr);
    vkFreeMemory(_device, _vertex_buffer_memory, nullptr);
    vkDestroyBuffer(_device, _index_buffer, nullptr);
    vkFreeMemory(_device, _index_buffer_memory, nullptr);

    for (int i = 0; i < Max_Frame_Number; ++i)
    {
      vkDestroySemaphore(_device, _image_available_semaphores[i], nullptr);
      vkDestroySemaphore(_device, _render_finished_semaphores[i], nullptr);
      vkDestroyFence(_device, _in_flight_fences[i], nullptr);
    }

    vkDestroyCommandPool(_device, _command_pool, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
    vkDestroyRenderPass(_device, _render_pass, nullptr);

    vkDestroyDevice(_device, nullptr);

    if (Enable_Validation_Layers)
      vkDestroyDebugUtilsMessengerEXT(_vk, _debug_messenger, nullptr);

    vkDestroySurfaceKHR(_vk, _surface, nullptr);
    vkDestroyInstance(_vk, nullptr);

    glfwDestroyWindow(_win);
    glfwTerminate();
  }

  void cleanup_swapchain()
  {
    for (auto framebuffer : _swapchain_framebuffers)
      vkDestroyFramebuffer(_device, framebuffer, nullptr);

    for (auto image_view : _swapchain_image_views)
      vkDestroyImageView(_device, image_view, nullptr);

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
  }


  /* Create Vulkan Instance */

  /**
   *  @brief  Create Vulkan Instance to connection with vulkan library.
   */
  void create_instance()
  {
    // fill vulkan instance info
    VkInstanceCreateInfo create_info = {};
    create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // set app info
    auto app_info = get_appliaction_information();
    create_info.pApplicationInfo     = &app_info;

    // set extensions
    auto extensions = get_instance_extensions();
    // check validation of extensions
    check_instance_extensions_support(extensions.size(), extensions.data());
    create_info.enabledExtensionCount   = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();

    // check and enable validation layer
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
    if constexpr (Enable_Validation_Layers)
    {
      check_validation_layers_support();
      create_info.enabledLayerCount   = Validation_Layers.size();
      create_info.ppEnabledLayerNames = Validation_Layers.data();

      // enable debug messenger
      // additional debug messenger use for vkCreateInstance and vkDestroyInstance
      debug_messenger_info = get_debug_utils_messenger_create_info();
      create_info.pNext = &debug_messenger_info;
    }

    // create vulkan instance
    VkResult res;
    if ((res = vkCreateInstance(&create_info, nullptr, &_vk)) != VK_SUCCESS)
      throw std::runtime_error(fmt::format("failed to create vulkan instance! VkResult: {}", (int64_t)res));
  }

  /**
   *  @brief   Optional information for create vulkan instance.
   *  @return  const VkApplicationInfo 
   *
   *  Describe applicaton and engine name and version.
   */
  const VkApplicationInfo get_appliaction_information() const noexcept
  {
    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = Application_Name;
    app_info.applicationVersion = Application_Version;
    app_info.pEngineName        = Engine_Name;
    app_info.engineVersion      = Engine_Version;
    app_info.apiVersion         = VK_API_VERSION_1_3;
    return app_info;
  }

  std::vector<const char*> get_instance_extensions()
  {
    // GLFW extension for interface with window system
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    // debug utils extension for message callback
    if constexpr (Enable_Validation_Layers)
      extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // other extentions
    extensions.append_range(_instance_extensions);

    return extensions;
  }

  /**
   *  @brief  Check supported extension and check validation.
   *  @param  extension_count  count of extension 
   *  @param  extensions       extensions array
   *  @throw  std::runtime_erorr  If @a extensions have unsupported extension.
   *
   *  This function will print supported extensions' name when Debug mode.
   */
  void check_instance_extensions_support(uint32_t extension_count, const char** extensions)
  {
    // get extension count
    uint32_t supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);

    // get extension details
    std::vector<VkExtensionProperties> supported_extensions(supported_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, supported_extensions.data());

#ifndef NDEBUG
    // print extensions info
    fmt::print(fg(fmt::color::green), "available vulkan instance extensions:\n");
    for (const auto& extension : supported_extensions)
      fmt::print(fg(fmt::color::green), "  {}\n", extension.extensionName);
#endif

    // check extensions validation
    for (uint32_t i = 0; i < extension_count; ++i)
    {
      auto it = std::find_if(supported_extensions.begin(), supported_extensions.end(),
                             [extensions, i] (const auto& supported_extension) {
                               return strcmp(supported_extension.extensionName, extensions[i]) == 0;
                             });
      if (it == supported_extensions.end())
        throw std::runtime_error(fmt::format("unsupported extension: {}", extensions[i]));
    }
  }

  /**
   *  @brief  Check validation layers' validation.
   *  @throw  std::runtime_error  If invalidated validation layers has.
   */
  void check_validation_layers_support()
  {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

#ifndef NDEBUG
    // print validation layers info
    fmt::print(fg(fmt::color::green), "available layers:\n");
    for (const auto& layer : layers)
      fmt::print(fg(fmt::color::green), "  {}\n", layer.layerName);
#endif

    for (auto layer : Validation_Layers)
    { auto it = std::find_if(layers.begin(), layers.end(),
                             [layer] (const auto& supported_layer) {
                               return strcmp(supported_layer.layerName, layer) == 0;
                             });
      if (it == layers.end())
        throw std::runtime_error(fmt::format("unsupported validaton layer: {}", layer));
    }
  }

  /* Set Vulkan Debug Messenger */

  VkDebugUtilsMessengerCreateInfoEXT get_debug_utils_messenger_create_info()
  {
    VkDebugUtilsMessengerCreateInfoEXT info = {};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;
    return info; 
  }

  void set_debug_messenger()
  {
    if (Enable_Validation_Layers == false)
      return;
    auto info = get_debug_utils_messenger_create_info();
    if (vkCreateDebugUtilsMessengerEXT(_vk, &info, nullptr, &_debug_messenger) != VK_SUCCESS)
      throw std::runtime_error("failed to create debug utils messenger extension");
  }

  VkResult vkCreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
  {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) 
      return func(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  void vkDestroyDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    VkDebugUtilsMessengerEXT                    messenger,
    const VkAllocationCallbacks*                pAllocator)
  {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
      func(instance, messenger, pAllocator);
  }

  /**
   *  @brief  Vulkan debug callback.
   *  @param  message_severity
   *  @param  message_type
   *  @param  callback_data
   *  @param  user_data
   *  @return  VK_FALSE  Always set this because VK_TRUE will cause
   *                     aborted with VK_ERROR_VALIDATION_FAILED_EXT error.
   *
   *  Use for filter message.
   */
  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
    VkDebugUtilsMessageTypeFlagsEXT             message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void*                                       user_data)
  {
    fmt::println("Validation Layer: {}", callback_data->pMessage);
    return VK_FALSE;
  }

  // Create Surface
  void create_surface()
  {
    if (glfwCreateWindowSurface(_vk, _win, nullptr, &_surface) != VK_SUCCESS)
      throw std::runtime_error("failed to create window surface!");
  }

  /* Select Physical Device */

  void select_physical_device()
  {
    // enumerate all physical device
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(_vk, &count, nullptr);
    if (count == 0)
      throw std::runtime_error("failed to find GPUs with vulkan support!");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(_vk, &count, devices.data());

#ifndef NDEBUG
    fmt::print(fg(fmt::color::green),
               "available physical devices:\n"
               "  name\t\t\t\t\tscore\n");
#endif

    // get all devices' score
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices)
    {
      int score = rate_device_suitability(device);
      candidates.insert(std::make_pair(score, device));
    }

    // select the best score and support specific queue feature's one
    for (const auto& device : std::ranges::views::reverse(candidates))
    {
      if (device.first > 0)
      {
        auto queue_family_indices = find_queue_families(device.second);
        if (queue_family_indices.is_complete() &&
            check_device_extensions_support(device.second))
        {
          auto swapchain_details = query_swapchain_support(device.second);
          if (!swapchain_details.formats.empty() &&
              !swapchain_details.present_modes.empty())
          {
            _physical_device = device.second;
            break;
          }
        }
      }
    }

    if (_physical_device == VK_NULL_HANDLE)
      throw std::runtime_error("failed to find a suitable GPU!");
  }

  /**
   *  @brief  Use score to judge which device is best.
   *  @param  device  physical device
   *  @return  score  score of device
   *
   *  select discrete GPU first, and must have geometry shader.
   */
  int rate_device_suitability(const VkPhysicalDevice& device)
  {
    int score = 0;

    VkPhysicalDeviceProperties properties = {};
    VkPhysicalDeviceFeatures   features   = {};
    vkGetPhysicalDeviceProperties(device, &properties);
    // TODO: after use more in features
    vkGetPhysicalDeviceFeatures(device, &features);

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      score += 1000;
    score += properties.limits.maxImageDimension2D;

    if (!features.geometryShader)
      score = 0;

#ifndef NDEBUG
    fmt::print(fg(fmt::color::green),
               "  {}\t{}\n", properties.deviceName, score);
#endif

    return score;
  }

  struct QueueFamilyIndices
  {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete()
    {
      return graphics_family.has_value() &&
             present_family.has_value();
    }
  };

  /**
   *  @brief  Find specific's queue family whether support VK_QUEUE_GRAPHICS_BIT.
   *  @param  device  physical device
   *  @return  queue_family_indices  indices of given device's specific queue family 
   */
  QueueFamilyIndices find_queue_families(const VkPhysicalDevice& device)
  {
    QueueFamilyIndices queue_family_indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queue_families.data());

    int i = 0;
    VkBool32 present_support = false;
    for (const auto& queue_family : queue_families)
    {
      // check graphics support
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        queue_family_indices.graphics_family = i;
     
      // check present support
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &present_support);
      if (present_support)
        queue_family_indices.present_family = i;

      // if get all queue families, quit
      if (queue_family_indices.is_complete())
        break;

      // HACK: some queue features may be in a same index,
      // so less queues best performance when queue is not much

      ++i;
    }

    return queue_family_indices;
  }

  bool check_device_extensions_support(VkPhysicalDevice device)
  {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
#ifndef NDEBUG
    fmt::print(fg(fmt::color::green), "available device extensions:\n");
    for (const auto& extension : extensions)
    {
      fmt::print(fg(fmt::color::green), "  {}\n", extension.extensionName);
    }
#endif
    for (auto required_extension : _device_extensions)
    {
      auto it = std::find_if(extensions.begin(), extensions.end(),
                             [required_extension] (const auto& extension)
                             {
                               return strcmp(required_extension, extension.extensionName) == 0;
                             });
      if (it == extensions.end())
        return false;
    }
    return true; 
  }

  struct SwapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   present_modes;
  };

  SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device)
  {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &count, nullptr);
    details.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &count, details.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &count, nullptr);
    details.present_modes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &count, details.present_modes.data());

    return details;
  }

  /* Create Logical Device */
  void create_logical_device()
  {
    // this should be found, if not the exception will throw in create physical device
    auto     queue_family_indices = find_queue_families(_physical_device);
    std::set<uint32_t> indices    =
    {
      queue_family_indices.graphics_family.value(),
      queue_family_indices.present_family.value()
    };
    float    priority             = 1.0f;
    
    // queue infos
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for (auto index : indices)
    {
      VkDeviceQueueCreateInfo queue_info = {};
      queue_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_info.queueFamilyIndex        = index;
      queue_info.queueCount              = 1;
      queue_info.pQueuePriorities        = &priority;
      queue_infos.push_back(queue_info);
    }

    // TODO: now, don't care feature info
    VkPhysicalDeviceFeatures features = {};

    // device info
    VkDeviceCreateInfo device_info      = {};
    device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount    = queue_infos.size();
    device_info.pQueueCreateInfos       = queue_infos.data();
    device_info.pEnabledFeatures        = &features;
    device_info.enabledExtensionCount   = _device_extensions.size();
    device_info.ppEnabledExtensionNames = _device_extensions.data();
   
    // create device
    // create device will automaticly create queues
    // and destroy also
    if (vkCreateDevice(_physical_device, &device_info, nullptr, &_device) != VK_SUCCESS)
      throw std::runtime_error("failed to create logical device!");

    // get graphics queue
    // here is just `get` the device queue which is already created in device creatation,
    // so get different queue families in same index is ok, it's just get value(or pointer).
    // and is also why here is vkGetDeviceQueue not vkCreateDeviceQueue and also not have vkDestroyDeviceQueue
    // reference: https://stackoverflow.com/questions/75781176/vulkan-get-multiple-queues-from-the-same-family
    vkGetDeviceQueue(_device, queue_family_indices.graphics_family.value(), 0, &_graphics_queue);
    vkGetDeviceQueue(_device, queue_family_indices.present_family.value(), 0, &_present_queue);
  }

  /* Create Swap Chain */

  VkSurfaceFormatKHR get_best_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
  {
    VkSurfaceFormatKHR format = {};
    auto it = std::find_if(formats.begin(), formats.end(),
                           [] (const auto& format)
                           {
                             return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                           });
    if (it != formats.end())
      return *it;
    return formats[0];
  }

  /**
   *  @brief  Get the best present mode of swap chain.
   *  @param  faster  Use VK_PRESENT_MODE_MAILBOX_KHR if true.
   *  @return  The best present mode you want.
   *
   *  you can set faster is false if you want to save energy, such as mobile devices.
   */
  VkPresentModeKHR get_best_present_mode(const std::vector<VkPresentModeKHR>& present_modes, bool faster = true) const noexcept
  {
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR; // which is guaranteed to be available
    auto it = std::find_if(present_modes.begin(), present_modes.end(),
                           [] (const auto& mode)
                           {
                             return mode == VK_PRESENT_MODE_MAILBOX_KHR;
                           });
    if (it != present_modes.end())
      mode = *it;
    return mode;
  }
  
  VkExtent2D get_suitable_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
  {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
      return capabilities.currentExtent;

    int width, height;
    glfwGetFramebufferSize(_win, &width, &height);

    VkExtent2D actual_extent = 
    { 
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)
    };

    actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
  }

  void create_swapchain(VkSwapchainKHR old_swapchain = VK_NULL_HANDLE)
  {
    // get info of swap chain
    auto     details        = query_swapchain_support(_physical_device);
    auto     surface_format = get_best_surface_format(details.formats);
    auto     present_mode   = get_best_present_mode(details.present_modes);
    auto     extent         = get_suitable_swap_extent(details.capabilities);
    uint32_t image_count    = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 &&
        image_count > details.capabilities.maxImageCount)
      image_count = details.capabilities.maxImageCount;

    // fill swap chain info
    VkSwapchainCreateInfoKHR info = {};
    info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface                  = _surface;
    info.minImageCount            = image_count;
    info.imageFormat              = surface_format.format;
    info.imageColorSpace          = surface_format.colorSpace;
    info.imageExtent              = extent;
    info.imageArrayLayers         = 1;
    info.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.oldSwapchain             = old_swapchain;

    // get queue families and set for swap chain
    auto queue_family_indices = find_queue_families(_physical_device);
    uint32_t indices[] = { queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value() };
    if (queue_family_indices.graphics_family != queue_family_indices.present_family)
    {
      // image can be used across multiple queue families witout explicit ownership transfers
      info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
      info.queueFamilyIndexCount = 2;
      info.pQueueFamilyIndices   = indices;
    }
    else
    {
      // image is owned by one queue family at a time
      // and ownership must be explicitly transferred before using it
      // in another queue family.
      // for best performance
      info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = details.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = present_mode;
    info.clipped        = VK_TRUE;
    // TODO: resize window use it
    info.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_device, &info, nullptr, &_swapchain) != VK_SUCCESS)
      throw std::runtime_error("failed to create swap chain!");

    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr);
    _swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data());
    _swapchain_image_format = surface_format.format;
    _swapchain_extent       = extent;
  }

  // Create Image Views
  void create_image_views()
  {
    _swapchain_image_views.resize(_swapchain_images.size());
    for (size_t i = 0; i < _swapchain_image_views.size(); ++i)
    {
      VkImageViewCreateInfo info           = {};
      info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.image                           = _swapchain_images[i];
      info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      info.format                          = _swapchain_image_format;
      info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      info.subresourceRange.baseMipLevel   = 0;
      info.subresourceRange.levelCount     = 1;
      info.subresourceRange.baseArrayLayer = 0;
      info.subresourceRange.layerCount     = 1;
      if (vkCreateImageView(_device, &info, nullptr, &_swapchain_image_views[i]) != VK_SUCCESS)
        throw std::runtime_error("failed to create image view!");
    }
  }

    /* Create Graphics Pipeline */
    void create_render_pass()
    {
      VkAttachmentDescription color_attachment = {};
      color_attachment.format         = _swapchain_image_format;
      color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
      color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
      color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
      color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      VkAttachmentReference color_attachment_reference =
      {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };

      VkSubpassDescription sub_pass =
      {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment_reference,
      };

      VkSubpassDependency dependency =
      {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      };

      VkRenderPassCreateInfo render_pass_info =
      {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color_attachment,
        .subpassCount    = 1,
        .pSubpasses      = &sub_pass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
      };
      if (vkCreateRenderPass(_device, &render_pass_info, nullptr, &_render_pass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");
    }

    /* Create Pipeline */

    static std::vector<char> read_spv_file(const std::string& filename)
    {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);
      if (!file.is_open())
        throw std::runtime_error(fmt::format("failed to open file: {}!", filename));

      size_t file_size = (size_t)file.tellg();
      std::vector<char> buffer(file_size);

      file.seekg(0);
      file.read(buffer.data(), file_size);

      file.close();
      return buffer;
    }

    VkShaderModule create_shader_module(const std::vector<char>& shader_bytecode)
    {
      VkShaderModuleCreateInfo info = {};
      info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      info.codeSize                 = shader_bytecode.size();
      info.pCode                    = reinterpret_cast<const uint32_t*>(shader_bytecode.data());
      VkShaderModule module;
      if (vkCreateShaderModule(_device, &info, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");
      return module;
    }

    inline VkShaderModule create_shader_module(const std::string& filename)
    {
      return create_shader_module(read_spv_file(filename));
    }

    void create_graphics_pipeline()
    {
      // programmable stages
      auto vertex_shader_module   = create_shader_module("shader/vertex.spv");
      auto fragment_shader_module = create_shader_module("shader/fragment.spv");

      VkPipelineShaderStageCreateInfo vertex_shader_stage_info   = {};
      vertex_shader_stage_info.sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      vertex_shader_stage_info.stage    = VK_SHADER_STAGE_VERTEX_BIT;
      vertex_shader_stage_info.module   = vertex_shader_module;
      vertex_shader_stage_info.pName    = "main";
      VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
      fragment_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      fragment_shader_stage_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
      fragment_shader_stage_info.module = fragment_shader_module;
      fragment_shader_stage_info.pName  = "main";
      VkPipelineShaderStageCreateInfo shader_stages[] = 
      {
        vertex_shader_stage_info,
        fragment_shader_stage_info
      };

      // fixed-function stages

      // dynamic state
      std::vector<VkDynamicState> dynamic_states =
      {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };
      VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
      dynamic_state_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      dynamic_state_info.dynamicStateCount = dynamic_states.size();
      dynamic_state_info.pDynamicStates    = dynamic_states.data();

      // vertex input
      auto binding_description    = Vertex::get_binding_description();
      auto attribute_descriptions = Vertex::get_attribute_descriptions();
      VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {};
      vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertex_input_state_info.vertexBindingDescriptionCount   = 1;
      vertex_input_state_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
      vertex_input_state_info.pVertexBindingDescriptions      = &binding_description;
      vertex_input_state_info.pVertexAttributeDescriptions    = attribute_descriptions.data();

      // input  assembly
      VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {};
      input_assembly_state_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      input_assembly_state_info.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      input_assembly_state_info.primitiveRestartEnable = VK_FALSE;

      // viewport and scissor
      VkViewport viewport = 
      {
        .width    = (float)_swapchain_extent.width,
        .height   = (float)_swapchain_extent.height,
        .maxDepth = 1.0f
      };
      VkRect2D scissor = 
      {
        .extent = _swapchain_extent
      };
      // viewport and scissor will be set at drawing time
      VkPipelineViewportStateCreateInfo viewport_state_info = 
      {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
      };

      // rasterizer
      VkPipelineRasterizationStateCreateInfo rasterizer_state_info =
      {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
      };

      // multisampling
      // TODO: after discuss multisample
      VkPipelineMultisampleStateCreateInfo multisample_state_info = 
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
      };

      // TODO: after discuss depth and stencil test

      // color blending
      VkPipelineColorBlendAttachmentState color_blend_attachment_state =
      {
        .blendEnable         = VK_FALSE,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                               VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT,
      };
      VkPipelineColorBlendStateCreateInfo color_blend_state_info =
      {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment_state,
      };

      // pipeline layout
      // TODO: currently is empty
      VkPipelineLayoutCreateInfo pipeline_layout_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      };
      if (vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");

      // create graphics pipeline
      VkGraphicsPipelineCreateInfo graphics_pipeline_info =
      {
        .sType                = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount           = 2,
        .pStages              = shader_stages,
        .pVertexInputState    = &vertex_input_state_info,
        .pInputAssemblyState  = &input_assembly_state_info,
        .pViewportState       = &viewport_state_info,
        .pRasterizationState  = &rasterizer_state_info,
        .pMultisampleState    = &multisample_state_info,
        .pColorBlendState     = &color_blend_state_info,
        .pDynamicState        = &dynamic_state_info,
        .layout               = _pipeline_layout,
        .renderPass           = _render_pass,
        .subpass              = 0,
        .basePipelineHandle   = VK_NULL_HANDLE,
        .basePipelineIndex    = -1,
      };
      if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &graphics_pipeline_info, nullptr, &_pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline!");

      vkDestroyShaderModule(_device, vertex_shader_module, nullptr);
      vkDestroyShaderModule(_device, fragment_shader_module, nullptr);
    }

    // Create Framebuffers
    void create_frambuffers()
    {
      _swapchain_framebuffers.resize(_swapchain_image_views.size());
      for (auto i = 0; i < _swapchain_image_views.size(); ++i)
      {
        VkImageView attachments[] =
        {
          _swapchain_image_views[i],
        };
        VkFramebufferCreateInfo info =
        {
          .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass      = _render_pass,
          .attachmentCount = 1,
          .pAttachments    = attachments,
          .width           = _swapchain_extent.width,
          .height          = _swapchain_extent.height,
          .layers          = 1,
        };
        if (vkCreateFramebuffer(_device, &info, nullptr, &_swapchain_framebuffers[i]) != VK_SUCCESS)
          throw std::runtime_error("failed to create framebuffers!");
      }
    }

  // Create Vertex Buffer and Index Buffer
  void create_buffer(
    VkDeviceSize          size,
    VkBufferUsageFlags    usage,
    VkMemoryPropertyFlags properties,
    VkBuffer&             buffer,
    VkDeviceMemory&       buffer_memory)
  {
    // create buffer
    VkBufferCreateInfo info =
    {
      .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size        = size,
      .usage       = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (vkCreateBuffer(_device, &info, nullptr, &buffer) != VK_SUCCESS)
      throw std::runtime_error("failed to create vertex buffer!");

    // memory requirement
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(_device, buffer, &mem_req);

    // allocate memory
    VkMemoryAllocateInfo alloc_info =
    {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = mem_req.size,
      .memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties),
    };
    if (vkAllocateMemory(_device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS)
      throw std::runtime_error("failed to allocate vertex buffer memory!");

    // assoicate buffer and memory
    vkBindBufferMemory(_device, buffer, buffer_memory, 0);
  }

  void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) 
  {
    // create temporary command buffer to transfer data from stage buffer to device local buffer
    VkCommandBufferAllocateInfo command_buffer_allocate_info =
    {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = _command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(_device, &command_buffer_allocate_info, &command_buffer);

    // record start transfer command
    VkCommandBufferBeginInfo begin_info =
    {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

    // record transfer data command
    VkBufferCopy copy_region =
    {
      .size = size,
    };
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);

    // end record command
    vkEndCommandBuffer(command_buffer);

    // submit command
    VkSubmitInfo submit_info =
    {
      .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers    = &command_buffer,
    };
    vkQueueSubmit(_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

    // wait transfer to complete
    vkQueueWaitIdle(_graphics_queue);

    // free temporary command buffer
    vkFreeCommandBuffers(_device, _command_pool, 1, &command_buffer);
  }

  void create_GPU_buffer(
    const void*           data,
    uint32_t              size,
    VkMemoryPropertyFlags memory_flags,
    VkBuffer&             buffer,
    VkDeviceMemory&       buffer_memory)
  {
    // create stage buffer
    VkBuffer stage_buffer;
    VkDeviceMemory stage_buffer_memory;
    create_buffer(
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      stage_buffer,
      stage_buffer_memory);

    // fill stage buffer
    // HACK: use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT resolve driver problems.
    // link: https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html
    void* mapped_data;
    vkMapMemory(_device, stage_buffer_memory, 0, size, 0, &mapped_data);
    memcpy(mapped_data, data, size);
    vkUnmapMemory(_device, stage_buffer_memory);

    // create device local vertex buffer
    create_buffer(
      size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | memory_flags,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      buffer,
      buffer_memory);
    
    // copy data to device local buffer
    copy_buffer(stage_buffer, buffer, size);

    // free stage buffer resources
    vkDestroyBuffer(_device, stage_buffer, nullptr);
    vkFreeMemory(_device, stage_buffer_memory, nullptr);
  }

  void create_vertex_buffer()
  {
    auto size = sizeof(_vertices[0]) * _vertices.size();
    create_GPU_buffer(_vertices.data(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _vertex_buffer, _vertex_buffer_memory);
  }

  void create_index_buffer()
  {
    auto size = sizeof(_indices[0]) * _indices.size();
    create_GPU_buffer(_indices.data(), size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, _index_buffer, _index_buffer_memory);
  }
  
  /**
   *  @brief  Get index of expected memory type and properties.
   *
   *  @param  type_filter  expected memory type
   *  @param  properties   expected memory properties
   *
   *  @return  index of expected memory type
   *
   *  @throw std::runtime_error  If not meet the requirements.
   */
  uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
  {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(_physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
      if (type_filter & (1 << i) &&
          (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
        return i; 

    throw std::runtime_error("failed to find suitable memory type!");
  }

    // Create Command Buffers
    void create_command_pool()
    {
      auto queue_family_indices = find_queue_families(_physical_device);

      VkCommandPoolCreateInfo info =
      {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family_indices.graphics_family.value(),
      };
      if (vkCreateCommandPool(_device, &info, nullptr, &_command_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");
    }

    void create_command_buffers()
    {
      _command_buffers.resize(Max_Frame_Number);
      VkCommandBufferAllocateInfo info =
      {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = _command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (uint32_t)_command_buffers.size(),
      };
      if (vkAllocateCommandBuffers(_device, &info, _command_buffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // Create Sync objects
    void create_sync_objects()
    {
      _image_available_semaphores.resize(Max_Frame_Number);
      _render_finished_semaphores.resize(Max_Frame_Number);
      _in_flight_fences.resize(Max_Frame_Number);

      VkSemaphoreCreateInfo info = 
      {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };

      VkFenceCreateInfo fence_info =
      {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };

      for (size_t i = 0; i < Max_Frame_Number; ++i)
      {
        if (vkCreateSemaphore(_device, &info, nullptr, &_image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_device, &info, nullptr, &_render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_device, &fence_info, nullptr, &_in_flight_fences[i])         != VK_SUCCESS)
          throw std::runtime_error("failed to create sync objects!");
      }
    }

    // Draw Frame
    void draw_frame()
    {
      // waiting for previous frame
      vkWaitForFences(_device, 1, &_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);

      // acquiring an image from the swap chain
      uint32_t image_index;
      VkResult res = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _image_available_semaphores[_current_frame], VK_NULL_HANDLE, &image_index);
      if (res != VK_SUCCESS)
        throw std::runtime_error("failed to acquire swap chain image!");

      // reset fence to make wait next submit
      vkResetFences(_device, 1, &_in_flight_fences[_current_frame]);

      // recording the command buffer
      vkResetCommandBuffer(_command_buffers[_current_frame], 0);
      record_command_buffer(_command_buffers[_current_frame], image_index);

      // submit the command buffer
      VkSemaphore wait_semaphores[] = { _image_available_semaphores[_current_frame] };
      VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
      VkSemaphore signal_semaphores[] = { _render_finished_semaphores[_current_frame] };
      VkSubmitInfo info =
      {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = wait_semaphores,
        .pWaitDstStageMask    = wait_stages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &_command_buffers[_current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = signal_semaphores,
      };
      if (vkQueueSubmit(_graphics_queue, 1, &info, _in_flight_fences[_current_frame]) != VK_SUCCESS) 
        throw std::runtime_error("failed to submit draw command buffer!");

      // presentation
      VkSwapchainKHR swapchains[] = { _swapchain };
      VkPresentInfoKHR presentation_info =
      {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = signal_semaphores,
        .swapchainCount     = 1,
        .pSwapchains        = swapchains,
        .pImageIndices      = &image_index,
      };

      res = vkQueuePresentKHR(_present_queue, &presentation_info);
      if (res != VK_SUCCESS)
        throw std::runtime_error("failed to present swapchain image!");

      // move to next frame
      _current_frame = ++_current_frame % Max_Frame_Number;
    }

    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index)
    {
      VkCommandBufferBeginInfo begin_info =
      {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      };
      if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("failed to begin command buffer!");

      VkClearValue clear_value = { (float)32/255, (float)33/255, (float)36/255, 1.f };
      VkRenderPassBeginInfo render_pass_begin_info =
      {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = _render_pass,
        .framebuffer = _swapchain_framebuffers[image_index],
        .renderArea  = 
        {
          .offset = { 0, 0 },
          .extent = _swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues    = &clear_value,
      };
      vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

      VkViewport viewport =
      {
        .width    = (float)_swapchain_extent.width,
        .height   = (float)_swapchain_extent.height,
        .maxDepth = 1.f,
      };
      vkCmdSetViewport(command_buffer, 0, 1, &viewport);

      VkRect2D scissor = 
      {
        .offset = { 0, 0 },
        .extent = _swapchain_extent,
      };
      vkCmdSetScissor(command_buffer, 0, 1, &scissor);

      VkBuffer     vertex_buffers[] = { _vertex_buffer };
      VkDeviceSize offsets[]        = { 0 };
      vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
      vkCmdBindIndexBuffer(command_buffer, _index_buffer, 0, VK_INDEX_TYPE_UINT16);

      vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);

      vkCmdEndRenderPass(command_buffer);

      if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to end command buffer!");
    }

private:
  // window
  GLFWwindow* _win                         = nullptr;
  static constexpr uint32_t Window_Height  = 600;
  static constexpr uint32_t Window_Width   = 800;

  // vulkan instance
  VkInstance _vk;
  static constexpr char     Application_Name[]  = "Vulkan Triangle";
  static constexpr char     Engine_Name[]       = "Galgame Engine";
  static constexpr uint32_t Application_Version = VK_MAKE_API_VERSION(0, 0, 0, 0);
  static constexpr uint32_t Engine_Version      = VK_MAKE_API_VERSION(0, 0, 0, 0);

  const std::vector<const char*> _instance_extensions =
  {
    // VK_EXT_swapchain_maintenance_1 extension need these
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
  };

  // validation layers
#ifdef NDEBUG
  static constexpr bool          Enable_Validation_Layers = false;
#else
  static constexpr bool          Enable_Validation_Layers = true;
  const std::vector<const char*> Validation_Layers        = { "VK_LAYER_KHRONOS_validation" };
  VkDebugUtilsMessengerEXT       _debug_messenger         = VK_NULL_HANDLE;
#endif
  
  // window surface
  VkSurfaceKHR _surface = VK_NULL_HANDLE;

  // physical device
  VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
  const std::vector<const char*> _device_extensions = 
  {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    // swapchain maintenance extension can auto recreate swapchain
    VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
  };

  // logical device
  VkDevice _device = VK_NULL_HANDLE;

  // queue families
  VkQueue _graphics_queue = VK_NULL_HANDLE;
  VkQueue _present_queue = VK_NULL_HANDLE;

  // swap chain
  VkSwapchainKHR       _swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> _swapchain_images;
  VkFormat             _swapchain_image_format;
  VkExtent2D           _swapchain_extent;

  // image views
  std::vector<VkImageView> _swapchain_image_views;

  // graphics pipeline
  VkRenderPass     _render_pass     = VK_NULL_HANDLE;
  VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;
  VkPipeline       _pipeline        = VK_NULL_HANDLE;

  // framebuffers
  std::vector<VkFramebuffer> _swapchain_framebuffers;

  // frame resources
  static constexpr int Max_Frame_Number = 2;
  uint32_t _current_frame = 0;

  // command buffers
  VkCommandPool   _command_pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> _command_buffers;

  // sync objects
  std::vector<VkSemaphore> _image_available_semaphores;
  std::vector<VkSemaphore> _render_finished_semaphores;
  std::vector<VkFence>     _in_flight_fences;

  // vertices and indices 
  const std::vector<Vertex> _vertices =
  {
    { { -.5f, -.5f }, { 1.f, 0.f, 0.f } },
    { {  .5f, -.5f }, { 0.f, 1.f, 0.f } },
    { {  .5f,  .5f }, { 0.f, 0.f, 1.f } },
    { { -.5f,  .5f }, { 1.f, 1.f, 1.f } },
  };
  const std::vector<uint16_t> _indices =
  {
    0, 1, 2,
    0, 2, 3,
  };
  VkBuffer       _vertex_buffer        = VK_NULL_HANDLE;
  VkDeviceMemory _vertex_buffer_memory = VK_NULL_HANDLE;
  VkBuffer       _index_buffer         = VK_NULL_HANDLE;
  VkDeviceMemory _index_buffer_memory  = VK_NULL_HANDLE;
};

int main()
{
  App app;

  try 
  {
    app.run();
  } 
  catch (const std::exception& e)
  {
    spdlog::error(e.what());
    exit(EXIT_FAILURE);
  }
}
