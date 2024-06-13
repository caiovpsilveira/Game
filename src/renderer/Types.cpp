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

    VmaAllocationCreateInfo allocationCreateInfo {.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
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
Mesh::Mesh(vk::Device device,
           vk::CommandBuffer transferCmd,
           VmaAllocator allocator,
           std::span<const Vertex> vertices,
           std::span<const uint32_t> indices)
{
    // Allocate device buffers
    const vk::DeviceSize vertexBufferSize = std::size(vertices) * sizeof(vertices[0]);
    const vk::DeviceSize indexBufferSize = std::size(indices) * sizeof(indices[0]);

    m_vertexBuffer = AllocatedBuffer(allocator,
                                     vertexBufferSize,
                                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                         vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                     VMA_MEMORY_USAGE_GPU_ONLY);

    m_indexBuffer = AllocatedBuffer(allocator,
                                    indexBufferSize,
                                    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    VMA_MEMORY_USAGE_GPU_ONLY);

    // Retrieve the vertex buffer address
    vk::BufferDeviceAddressInfo bufferDeviceAddressInfo {.sType = vk::StructureType::eBufferDeviceAddressInfo,
                                                         .pNext = nullptr,
                                                         .buffer = m_vertexBuffer.buffer()};
    m_vertexBufferAddress = device.getBufferAddress(bufferDeviceAddressInfo);

    // Copy vertex and index buffer data from host to a device staging buffer
    // The staging buffer will be a contiguous buffer containing [vertexBufferData, indexBufferData]
    AllocatedBuffer stagingBuffer(allocator,
                                  vertexBufferSize + indexBufferSize,
                                  vk::BufferUsageFlagBits::eTransferSrc,
                                  VMA_MEMORY_USAGE_CPU_ONLY);

    VmaAllocationInfo stagingAllocationInfo;
    vmaGetAllocationInfo(allocator, stagingBuffer.allocation(), &stagingAllocationInfo);
    auto& stagingData = stagingAllocationInfo.pMappedData;
    // copy vertex buffer data
    std::memcpy(stagingData, vertices.data(), vertexBufferSize);
    // copy index buffer data
    std::memcpy((char*) stagingData + vertexBufferSize, indices.data(), indexBufferSize);

    // Copy the device staging buffer data to the device vertex buffer and index buffer
    vk::BufferCopy vertexCopyRegion {.srcOffset = 0, .dstOffset = 0, .size = vertexBufferSize};
    transferCmd.copyBuffer(stagingBuffer.buffer(), m_vertexBuffer.buffer(), vertexCopyRegion);

    vk::BufferCopy indexCopyRegion {.srcOffset = vertexBufferSize, .dstOffset = 0, .size = indexBufferSize};
    transferCmd.copyBuffer(stagingBuffer.buffer(), m_indexBuffer.buffer(), indexCopyRegion);
}
// End Mesh

}   // namespace renderer
