#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace Vulkan
{
  struct Vertex
  {
    glm::vec2 pos;
    glm::vec3 col;
    glm::vec2 tex;
  };

  struct Mesh
  {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
  };

  inline Mesh get_rectangle_mesh(uint32_t width, uint32_t height, glm::vec3 color)
  {
    return
    {
      {
        { -1.f,  1.f }, color, { 0.f, 1.f },
        {  1.f,  1.f }, color, { 1.f, 1.f },
        {  1.f, -1.f }, color, { 1.f, 0.f },
        { -1.f, -1.f }, color, { 0.f, 0.f },
      },
      {
        { 0, 1, 2 },
        { 0, 2, 3 },
      }
    };
  }
}
