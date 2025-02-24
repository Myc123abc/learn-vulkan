#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <stdexcept>
#include <map>
#include <optional>
#include <ranges>
#include <set>

#include <spdlog/spdlog.h>
#include <fmt/color.h>

class App
{
public:
  void run()
  {
    init_window();
    init_vulkan();
    // TODO: when use loop enable
    // main_loop();
    cleanup();
  }

private:
  void init_window()
  {
    if (glfwInit() == GLFW_FALSE)
      throw std::runtime_error("failed to init GLFW!");

    // don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // TODO: currently, only disable resize window
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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
    create_swap_chain();
  }

  void main_loop() 
  {
    while (!glfwWindowShouldClose(_win))
    {
      glfwPollEvents();
    }
  }

  void cleanup() 
  {
    vkDestroySwapchainKHR(_device, _swap_chain, nullptr);
    vkDestroyDevice(_device, nullptr);

    if (Enable_Validation_Layers)
      vkDestroyDebugUtilsMessengerEXT(_vk, _debug_messenger, nullptr);

    vkDestroySurfaceKHR(_vk, _surface, nullptr);
    vkDestroyInstance(_vk, nullptr);

    glfwDestroyWindow(_win);
    glfwTerminate();
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
    auto extensions = get_extensions();
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

  std::vector<const char*> get_extensions()
  {
    // GLFW extension for interface with window system
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    // debug utils extension for message callback
    if constexpr (Enable_Validation_Layers)
      extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
          auto swap_chain_details = query_swapchain_support(device.second);
          if (!swap_chain_details.formats.empty() &&
              !swap_chain_details.present_modes.empty())
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

      // PERFORMANCE: some queue features may be in a same index,
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

  void create_swap_chain()
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

    if (vkCreateSwapchainKHR(_device, &info, nullptr, &_swap_chain) != VK_SUCCESS)
      throw std::runtime_error("failed to create swap chain!");

    vkGetSwapchainImagesKHR(_device, _swap_chain, &image_count, nullptr);
    _swap_chain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_device, _swap_chain, &image_count, _swap_chain_images.data());
    _swap_chain_image_format = surface_format.format;
    _swap_chain_extent       = extent;
  }

private:
  // window
  GLFWwindow* _win                         = nullptr;
  static constexpr uint32_t Window_Height  = 800;
  static constexpr uint32_t Window_Width   = 600;

  // vulkan instance
  VkInstance _vk;
  static constexpr char     Application_Name[]  = "Vulkan Triangle";
  static constexpr char     Engine_Name[]       = "Galgame Engine";
  static constexpr uint32_t Application_Version = VK_MAKE_API_VERSION(0, 0, 0, 0);
  static constexpr uint32_t Engine_Version      = VK_MAKE_API_VERSION(0, 0, 0, 0);

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
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  // logical device
  VkDevice _device = VK_NULL_HANDLE;

  // queue families
  VkQueue _graphics_queue = VK_NULL_HANDLE;
  VkQueue _present_queue = VK_NULL_HANDLE;

  // swap chain
  VkSwapchainKHR       _swap_chain = VK_NULL_HANDLE;
  std::vector<VkImage> _swap_chain_images;
  VkFormat             _swap_chain_image_format;
  VkExtent2D           _swap_chain_extent;
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
