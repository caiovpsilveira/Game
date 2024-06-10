#include "VmaAllocatorUnique.hpp"

// libs
#include <vulkan/vulkan.hpp>

// std
#include <utility>

namespace core
{

VmaAllocatorUnique::VmaAllocatorUnique(const VmaAllocatorCreateInfo& createInfo)
{
    vk::Result result = static_cast<vk::Result>(vmaCreateAllocator(&createInfo, &m_allocator));
    // To be consistent with vulkan.hpp, throw an exception if != vk::Success
    vk::detail::resultCheck(result, "vmaCreateAllocator");
}

VmaAllocatorUnique::VmaAllocatorUnique(VmaAllocatorUnique&& rhs) noexcept
    : m_allocator(std::exchange(rhs.m_allocator, nullptr))
{}

VmaAllocatorUnique& VmaAllocatorUnique::operator=(VmaAllocatorUnique&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(m_allocator, rhs.m_allocator);
    }
    return *this;
}

VmaAllocatorUnique::~VmaAllocatorUnique() noexcept
{
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
    }
}

}   // namespace core
