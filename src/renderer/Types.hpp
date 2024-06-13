#ifndef RENDERER_TYPES_HPP
#define RENDERER_TYPES_HPP

// libs
#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// std
#include <array>
#include <cstdint>
#include <span>

namespace renderer
{

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription bindingDescription() noexcept;
    static std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions() noexcept;
};

class AllocatedBuffer
{
public:
    AllocatedBuffer() noexcept = default;
    AllocatedBuffer(VmaAllocator allocator,
                    vk::DeviceSize size,
                    vk::BufferUsageFlags usage,
                    VmaMemoryUsage memoryUsage);

    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    AllocatedBuffer(AllocatedBuffer&&) noexcept;
    AllocatedBuffer& operator=(AllocatedBuffer&&) noexcept;

    ~AllocatedBuffer() noexcept;

public:
    vk::Buffer buffer() const noexcept { return m_buffer; }

private:
    VmaAllocator m_allocator = nullptr;   // not owned
    vk::Buffer m_buffer;
    VmaAllocation m_allocation;
};

class Mesh
{
public:
    Mesh() noexcept = default;
    Mesh(vk::Device device,
         VmaAllocator allocator,
         std::span<const Vertex> vertices,
         std::span<const uint32_t> indices);

private:
    AllocatedBuffer m_vertexBuffer;
    vk::DeviceAddress m_vertexBufferAddress;
    AllocatedBuffer m_indexBuffer;
};

}   // namespace renderer

#endif
