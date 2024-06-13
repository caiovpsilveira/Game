#ifndef CORE_UNIQUE_VMA_ALLOCATOR
#define CORE_UNIQUE_VMA_ALLOCATOR

#include <vk_mem_alloc.h>

namespace core
{

class UniqueVmaAllocator
{
public:
    UniqueVmaAllocator() noexcept = default;
    UniqueVmaAllocator(const VmaAllocatorCreateInfo& createInfo);

    UniqueVmaAllocator(const UniqueVmaAllocator&) = delete;
    UniqueVmaAllocator& operator=(const UniqueVmaAllocator&) = delete;

    UniqueVmaAllocator(UniqueVmaAllocator&&) noexcept;
    UniqueVmaAllocator& operator=(UniqueVmaAllocator&&) noexcept;

    ~UniqueVmaAllocator() noexcept;

    constexpr explicit operator bool() const noexcept { return m_allocator != nullptr; }
    constexpr VmaAllocator operator*() const noexcept { return m_allocator; }
    constexpr VmaAllocator get() const noexcept { return m_allocator; }

private:
    VmaAllocator m_allocator = nullptr;
};

};   // namespace core

#endif
