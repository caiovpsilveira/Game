#ifndef RENDERER_TYPES_HPP
#define RENDERER_TYPES_HPP

// libs
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

// std
#include <array>

namespace renderer
{

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription bindingDescription() noexcept;
    static std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions() noexcept;
};

}   // namespace renderer

#endif
