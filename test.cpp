#include "Log.hpp"
#include "Vulkan.hpp"
#include <cstdlib>
#include <exception>
#include <memory>

using namespace Vulkan;

int main()
{
  try 
  {
    ApplicationInfo app_info =
    {
      .app_name       = "test",
      .app_version    = version(0, 0, 0),
      .engine_name    = "test",
      .engine_version = version(0, 0, 0),
      .vulkan_version = VK_API_VERSION_1_4,
    };

    VulkanCreateInfo create_info =
    {
      .width    = 800,
      .height   = 600,
      .title    = "test",
      .app_info = app_info,
    };

    auto vulkan = std::make_unique<class Vulkan>(create_info);
  }
  catch (const std::exception& e)
  {
    Log::error(e.what());
    exit(EXIT_FAILURE);
  }
}
