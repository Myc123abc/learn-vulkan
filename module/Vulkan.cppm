module;

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

export module Vulkan;

inline void throw_if(bool b, std::string_view msg)
{
  if (b) throw std::runtime_error(msg.data());
}

export class Vulkan final
{
public:
  void init(uint32_t width, uint32_t height, std::string_view title)
  {
    init_window(width, height, title);
  }

  void init_window(uint32_t width, uint32_t height, std::string_view title)
  {
    throw_if(glfwInit() == GLFW_FALSE, "Failed to init GLFW!");

    // don't create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
    throw_if(_window == nullptr, "Failed to create window!");
  }

private:
  // window
  GLFWwindow* _window;
};
