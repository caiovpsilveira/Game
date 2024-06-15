#include "Types.hpp"

namespace renderer
{

// Begin Vertex
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
// End Vertex

// Begin AllocatedBuffer
AllocatedBuffer::AllocatedBuffer(VmaAllocator allocator,
                                 vk::DeviceSize size,
                                 vk::BufferUsageFlags usage,
                                 VmaAllocatorCreateFlags allocationFlags,
                                 VmaMemoryUsage memoryUsage)
    : m_allocator(allocator)
{
    vk::BufferCreateInfo bufferCreateInfo {.sType = vk::StructureType::eBufferCreateInfo,
                                           .pNext = nullptr,
                                           .flags = {},
                                           .size = size,
                                           .usage = usage,
                                           .sharingMode = vk::SharingMode::eExclusive,
                                           .queueFamilyIndexCount = 0,
                                           .pQueueFamilyIndices = nullptr};

    VmaAllocationCreateInfo allocationCreateInfo {.flags = allocationFlags,
                                                  .usage = memoryUsage,
                                                  .requiredFlags = {},
                                                  .preferredFlags = {},
                                                  .memoryTypeBits = {},
                                                  .pool = nullptr,
                                                  .pUserData = nullptr,
                                                  .priority = 0.f};

    vk::detail::resultCheck(
        static_cast<vk::Result>(vmaCreateBuffer(m_allocator,
                                                reinterpret_cast<VkBufferCreateInfo*>(&bufferCreateInfo),
                                                &allocationCreateInfo,
                                                reinterpret_cast<VkBuffer*>(&m_buffer),
                                                &m_allocation,
                                                nullptr)),
        "vmaCreateBuffer");
}

AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& rhs) noexcept
    : m_allocator(std::exchange(rhs.m_allocator, nullptr))
    , m_buffer(rhs.m_buffer)
    , m_allocation(rhs.m_allocation)
{}

AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(m_allocator, rhs.m_allocator);
        std::swap(m_buffer, rhs.m_buffer);
        std::swap(m_allocation, rhs.m_allocation);
    }
    return *this;
}

AllocatedBuffer::~AllocatedBuffer() noexcept
{
    if (m_allocator != nullptr) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}
// End AllocatedBuffer

// Begin Mesh
Mesh::Mesh(vk::Device device, VmaAllocator allocator, vk::DeviceSize vertexBufferSize, vk::DeviceSize indexBufferSize)
{
    m_numIndices = indexBufferSize / sizeof(uint32_t);

    // Allocate device buffers
    m_vertexBuffer = AllocatedBuffer(allocator,
                                     vertexBufferSize,
                                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                         vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                     0,
                                     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    m_indexBuffer = AllocatedBuffer(allocator,
                                    indexBufferSize,
                                    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    0,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    // Retrieve the vertex buffer address
    vk::BufferDeviceAddressInfo bufferDeviceAddressInfo {.sType = vk::StructureType::eBufferDeviceAddressInfo,
                                                         .pNext = nullptr,
                                                         .buffer = m_vertexBuffer.buffer()};
    m_vertexBufferAddress = device.getBufferAddress(bufferDeviceAddressInfo);
}

// End Mesh

}   // namespace renderer
