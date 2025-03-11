#include "Log.hpp"
#include "Vulkan.hpp"

#include <cstdlib>
#include <exception>

int main()
{
  Vulkan vulkan;
  try 
  {
    vulkan.init(800, 600, "test");
  }
  catch (const std::exception& e)
  {
    Log::error(e.what());
    exit(EXIT_FAILURE);
  }
}
