#ifndef CORE_VMA_ALLOCATOR_UNIQUE
#define CORE_VMA_ALLOCATOR_UNIQUE

#include <vk_mem_alloc.h>

namespace core
{

class VmaAllocatorUnique
{
public:
    VmaAllocatorUnique() noexcept = default;
    VmaAllocatorUnique(const VmaAllocatorCreateInfo& createInfo);

    VmaAllocatorUnique(const VmaAllocatorUnique&) = delete;
    VmaAllocatorUnique& operator=(const VmaAllocatorUnique&) = delete;

    VmaAllocatorUnique(VmaAllocatorUnique&&) noexcept;
    VmaAllocatorUnique& operator=(VmaAllocatorUnique&&) noexcept;

    ~VmaAllocatorUnique() noexcept;

    constexpr explicit operator bool() const noexcept { return m_allocator != nullptr; }
    constexpr VmaAllocator operator*() const noexcept { return m_allocator; }

private:
    VmaAllocator m_allocator = nullptr;
};

};   // namespace core

#endif
