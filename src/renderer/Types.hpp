#ifndef RENDERER_TYPES_HPP
#define RENDERER_TYPES_HPP

// libs
#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// std
#include <array>
#include <cstdint>

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
                    VmaAllocatorCreateFlags allocationFlags,
                    VmaMemoryUsage memoryUsage);

    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    AllocatedBuffer(AllocatedBuffer&&) noexcept;
    AllocatedBuffer& operator=(AllocatedBuffer&&) noexcept;

    ~AllocatedBuffer() noexcept;

public:
    vk::Buffer buffer() const noexcept { return m_buffer; }
    VmaAllocation allocation() const noexcept { return m_allocation; }

private:
    VmaAllocator m_allocator = nullptr;   // not owned
    vk::Buffer m_buffer;
    VmaAllocation m_allocation;
};

class Mesh
{
public:
    Mesh() noexcept = default;
    // Only allocates the buffers on the device. The buffers still have to be filled with a staging buffer.
    // TODO: improve this?
    Mesh(vk::Device device, VmaAllocator allocator, vk::DeviceSize vertexBufferSize, vk::DeviceSize indexBufferSize);

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) = default;

    ~Mesh() noexcept = default;

public:
    vk::Buffer vertexBuffer() const noexcept { return m_vertexBuffer.buffer(); }
    vk::Buffer indexBuffer() const noexcept { return m_indexBuffer.buffer(); }
    uint32_t numIndices() const noexcept { return m_numIndices; }

private:
    AllocatedBuffer m_vertexBuffer;
    vk::DeviceAddress m_vertexBufferAddress;
    AllocatedBuffer m_indexBuffer;
    uint32_t m_numIndices;
};

struct TransferData {
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandBuffer commandBuffer;
    vk::UniqueFence fence;
};

struct FrameCommandData {
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandBuffer commandBuffer;
    vk::UniqueSemaphore swapchainSemaphore;
    vk::UniqueSemaphore renderSemaphore;
    vk::UniqueFence renderFence;
};

struct FrameData {
    FrameCommandData commandData;
};

}   // namespace renderer

#endif
