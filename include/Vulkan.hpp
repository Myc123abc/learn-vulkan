/*===-- include/Vulkan.hpp ----- Vulkan -----------------------------------===*\
|*                                                                            *|
|* Copyright (c) 2025 Ma Yuncong                                              *|
|* Licensed under the MIT License.                                            *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* This header declare the Vulkan instance.                                   *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string_view>
#include <optional>

namespace Vulkan
{

  /**
   * Describe the application and engine information.
   */
  struct ApplicationInfo
  {
    std::string_view app_name;       ///< applicaton name
    uint32_t         app_version;    ///< application version
    std::string_view engine_name;    ///< engine name
    uint32_t         engine_version; ///< engine version
    uint32_t         vulkan_version; ///< vulkan version
  };
  
  /**
   * Get version.
   *
   * @param major major version.
   * @param minor minor version.
   * @param patch patch version.
   * @return version.
   */
  inline uint32_t version(uint32_t major, uint32_t minor, uint32_t patch)
  {
    return VK_MAKE_API_VERSION(0, major, minor, patch);
  }
  
  /**
   * Create Vulkan object information.
   */
  struct VulkanCreateInfo
  {
    uint32_t width;                          ///< width of window
    uint32_t height;                         ///< height of window
    std::string_view title;                  ///< title of window
    std::optional<ApplicationInfo> app_info; ///< application information, can be empty.
  };
  
  /**
   * Vulkan Interface.
   */
  class Vulkan final
  {
  public:
    /**
     * Initialize Vulkan.
     *
     * @param info Vulkan create information. 
     */
    Vulkan(const VulkanCreateInfo& info);
  
  
  private:
    void init_window(uint32_t width, uint32_t height, std::string_view title);
    void init_vulkan(const VulkanCreateInfo& info);
  
    void create_vulkan_instance(const VulkanCreateInfo& info);
  
  private:
    GLFWwindow* _window;
  
    VkInstance _vulkan; 
  };

}
