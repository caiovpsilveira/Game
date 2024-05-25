#ifndef CORE_VULKAN_CONTEXT_H
#define CORE_VULKAN_CONTEXT_H

// std
#include <span>

// libs
#include <vulkan/vulkan.hpp>

struct SDL_Window;
struct VmaAllocator_T;

namespace core
{

class VulkanContext
{
public:
    VulkanContext() noexcept = default;
    VulkanContext(std::span<const char*> requiredInstanceSurfaceExtensions,
                  bool enableValidationLayersIfSupported,
                  bool enableDebugMessengerIfSupported,
                  SDL_Window* window);

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;

    ~VulkanContext() noexcept;

private:
    void createInstanceAndDebug(std::span<const char*> requiredInstanceExtensions,
                                bool enableValidationLayersIfSupported,
                                bool enableDebugMessengerIfSupported);

    void cleanup() noexcept;

private:
    vk::Instance m_instance = nullptr;
    vk::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::SurfaceKHR m_surface = nullptr;
    vk::PhysicalDevice m_physicalDevice = nullptr;
    vk::Device m_device = nullptr;
    VmaAllocator_T* m_allocator = nullptr;
    vk::SwapchainKHR m_swapchain = nullptr;
};

}   // namespace core

#endif
