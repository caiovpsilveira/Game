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
    VmaAllocationInfo allocationInfo() const noexcept;

private:
    VmaAllocator m_allocator = nullptr;   // not owned
    vk::Buffer m_buffer;
    VmaAllocation m_allocation;
};

class Mesh
{
public:
    // TODO: allow different index types other than uint32_t, for smaller meshes
    Mesh() noexcept = default;
    // Only allocates the buffers on the device. The buffers still have to be filled with a staging buffer.
    // TODO: improve this?
    Mesh(vk::Device device, VmaAllocator allocator, vk::DeviceSize vertexBufferSize, vk::DeviceSize indexBufferSize);

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

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

class Allocated2DImage
{
public:
    Allocated2DImage() noexcept = default;
    Allocated2DImage(VmaAllocator allocator,
                     vk::Format format,
                     const vk::Extent2D& extent,
                     vk::ImageTiling tiling,
                     vk::ImageUsageFlags usage,
                     VmaAllocatorCreateFlags allocationFlags,
                     VmaMemoryUsage memoryUsage);

    Allocated2DImage(const Allocated2DImage&) = delete;
    Allocated2DImage& operator=(const Allocated2DImage&) = delete;

    Allocated2DImage(Allocated2DImage&&) noexcept;
    Allocated2DImage& operator=(Allocated2DImage&&) noexcept;

    ~Allocated2DImage() noexcept;

public:
    vk::Image image() const noexcept { return m_image; }

private:
    VmaAllocator m_allocator = nullptr;   // not owned
    vk::Image m_image;
    VmaAllocation m_allocation;
};

struct TransferCommandData {
    vk::UniqueCommandPool commandPool;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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
    AllocatedBuffer ubo;
    vk::DescriptorSet uboDescriptorSet;   // owned by the pool
};

}   // namespace renderer

#endif
