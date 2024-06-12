#include "Types.hpp"

namespace renderer
{

vk::VertexInputBindingDescription Vertex::bindingDescription() noexcept
{
    return vk::VertexInputBindingDescription {.binding = 0,
                                              .stride = sizeof(Vertex),
                                              .inputRate = vk::VertexInputRate::eVertex};
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::attributeDescriptions() noexcept
{
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions;
    // pos
    attributeDescriptions[0] = {.location = 0,
                                .binding = 0,
                                .format = vk::Format::eR32G32Sfloat,
                                .offset = offsetof(Vertex, pos)};
    // color
    attributeDescriptions[1] = {.location = 1,
                                .binding = 0,
                                .format = vk::Format::eR32G32B32Sfloat,
                                .offset = offsetof(Vertex, color)};
    return attributeDescriptions;
}

}   // namespace renderer
