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
#include "VmaUsage.h"

#include <string_view>
#include <optional>
#include <vector>
#include <array>

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
    uint32_t         vulkan_version = -1; ///< vulkan version
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
    ~Vulkan();
  
  private:
    void init_window(uint32_t width, uint32_t height, std::string_view title);
    void init_vulkan(const VulkanCreateInfo& info);
  
    void create_vulkan_instance(const VulkanCreateInfo& info);
    void create_debug_messenger();
    void create_surface();
    void select_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_render_pass();
    void create_destriptor_set_layout();
    void create_pipeline();
    void create_framebuffer(); 
    void create_command_pool();
    void create_command_buffers();
    void create_buffers();

    static VkResult vkCreateDebugUtilsMessengerEXT(
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
    static void vkDestroyDebugUtilsMessengerEXT(
      VkInstance                                  instance,
      VkDebugUtilsMessengerEXT                    messenger,
      const VkAllocationCallbacks*                pAllocator)
    {
      auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
      if (func != nullptr)
        func(instance, messenger, pAllocator);
    }

  private:
    GLFWwindow* _window = nullptr;
  
    VkInstance _vulkan = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

    VkSurfaceKHR _surface = VK_NULL_HANDLE;

    VkPhysicalDevice _physical_device = VK_NULL_HANDLE;

    VkDevice      _device         = VK_NULL_HANDLE;
    VkQueue       _graphics_queue = VK_NULL_HANDLE;
    VkQueue       _present_queue  = VK_NULL_HANDLE;
    VmaAllocator  _vma_allocator  = VK_NULL_HANDLE;

    VkSwapchainKHR       _swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> _swapchain_images;
    VkFormat             _swapchain_image_format;
    VkExtent2D           _swapchain_image_extent;

    std::vector<VkImageView> _swapchain_image_views;

    VkRenderPass _render_pass = VK_NULL_HANDLE;

    VkDescriptorSetLayout _descriptor_set_layout = VK_NULL_HANDLE;

    VkPipeline       _pipeline        = VK_NULL_HANDLE;
    VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> _swapchain_framebuffers;

    VkCommandPool _command_pool = VK_NULL_HANDLE;

    static constexpr uint32_t Max_Frame_Number = 2;
    std::array<VkCommandBuffer, Max_Frame_Number> _command_buffers;

    // TODO: sub-allocation
    // VkBuffer      _buffer         = VK_NULL_HANDLE;
    // VmaAllocation _vma_allocation = VK_NULL_HANDLE;

    // HACK: create seperate buffer...
    VkBuffer      _vertex_buffer            = VK_NULL_HANDLE;
    VmaAllocation _vertex_buffer_allocation = VK_NULL_HANDLE;
    VkBuffer      _index_buffer             = VK_NULL_HANDLE;
    VmaAllocation _index_buffer_allocation  = VK_NULL_HANDLE;
    std::array<VkBuffer, Max_Frame_Number>      _uniform_buffers;
    std::array<VmaAllocation, Max_Frame_Number> _uniform_buffer_allocationss;

    // HACK: tmp func
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
  };

}
