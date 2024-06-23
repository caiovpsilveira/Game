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
    , m_allocation(rhs.m_allocation)
    , m_buffer(rhs.m_buffer)
{}

AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(m_allocator, rhs.m_allocator);
        std::swap(m_allocation, rhs.m_allocation);
        std::swap(m_buffer, rhs.m_buffer);
    }
    return *this;
}

AllocatedBuffer::~AllocatedBuffer() noexcept
{
    if (m_allocator != nullptr) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

VmaAllocationInfo AllocatedBuffer::allocationInfo() const noexcept
{
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(m_allocator, m_allocation, &allocationInfo);
    return allocationInfo;
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

// Begin Allocated2DImage
Allocated2DImage::Allocated2DImage(vk::Device device,
                                   VmaAllocator allocator,
                                   vk::Format format,
                                   const vk::Extent2D& extent,
                                   vk::ImageTiling tiling,
                                   vk::ImageUsageFlags usage,
                                   VmaAllocatorCreateFlags allocationFlags,
                                   VmaMemoryUsage memoryUsage)
    : m_allocator(allocator)
{
    vk::ImageCreateInfo imageCreateInfo {
        .sType = vk::StructureType::eImageCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {.width = extent.width, .height = extent.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    VmaAllocationCreateInfo allocationCreateInfo {.flags = allocationFlags,
                                                  .usage = memoryUsage,
                                                  .requiredFlags = {},
                                                  .preferredFlags = {},
                                                  .memoryTypeBits = {},
                                                  .pool = nullptr,
                                                  .pUserData = nullptr,
                                                  .priority = 0.f};

    vk::detail::resultCheck(
        static_cast<vk::Result>(vmaCreateImage(m_allocator,
                                               reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo),
                                               &allocationCreateInfo,
                                               reinterpret_cast<VkImage*>(&m_image),
                                               &m_allocation,
                                               nullptr)),
        "vmaCreateImage");

    vk::ImageViewCreateInfo imageViewCreateInfo {
        .sType = vk::StructureType::eImageViewCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .image = m_image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = {vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eIdentity},
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1}
    };

    try {
        m_imageView = device.createImageViewUnique(imageViewCreateInfo);
    } catch (...) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
        throw;
    }
}

Allocated2DImage::Allocated2DImage(Allocated2DImage&& rhs) noexcept
    : m_allocator(std::exchange(rhs.m_allocator, nullptr))
    , m_allocation(rhs.m_allocation)
    , m_image(rhs.m_image)
    , m_imageView(std::exchange(rhs.m_imageView, {}))
{}

Allocated2DImage& Allocated2DImage::operator=(Allocated2DImage&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(m_allocator, rhs.m_allocator);
        std::swap(m_allocation, rhs.m_allocation);
        std::swap(m_image, rhs.m_image);
        std::swap(m_imageView, rhs.m_imageView);
    }
    return *this;
}

Allocated2DImage::~Allocated2DImage() noexcept
{
    if (m_allocator) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
}
// End Allocated2DImage

}   // namespace renderer
