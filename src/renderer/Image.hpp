#ifndef RENDERER_IMAGE_HPP
#define RENDERER_IMAGE_HPP

// libs
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// std
#include <memory>

namespace renderer
{

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
    VmaAllocation m_allocation;
    vk::Image m_image;
};

class AllocatedTexture
{
public:
    AllocatedTexture() noexcept = default;
    AllocatedTexture(std::shared_ptr<const Allocated2DImage> image,
                     vk::UniqueImageView imageView,
                     vk::DescriptorSet descriptor);

    AllocatedTexture(const AllocatedTexture&) = delete;
    AllocatedTexture& operator=(const AllocatedTexture&) = delete;

    AllocatedTexture(AllocatedTexture&&) noexcept = default;
    AllocatedTexture& operator=(AllocatedTexture&&) noexcept = default;

    ~AllocatedTexture() noexcept = default;

public:
    vk::Image image() const noexcept { return m_image->image(); }
    vk::ImageView imageView() const noexcept { return *m_imageView; }
    vk::DescriptorSet descriptor() const noexcept { return m_descriptor; }

private:
    std::shared_ptr<const Allocated2DImage> m_image;
    vk::UniqueImageView m_imageView;
    vk::DescriptorSet m_descriptor;   // owned by the pool;
};

}   // namespace renderer

#endif
