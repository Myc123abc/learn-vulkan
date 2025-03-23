#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Vulkan 
{
glm::mat4  model = glm::mat4(1.f);
glm::mat4  view  = glm::lookAt(glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
// glm::mat4  proj  = glm::perspective(glm::radians(90.f), 800 / (float)600, 0.1f, 100.f);
glm::mat4  proj  = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 100.f);
// glm::mat4  view  = glm::mat4(1.f);
// glm::mat4  proj  = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 100.f);

}
