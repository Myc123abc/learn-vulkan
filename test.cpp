#include <cstdlib>
#include <exception>

import Vulkan;
import Log;

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
