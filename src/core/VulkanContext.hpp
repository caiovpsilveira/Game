#ifndef CORE_VULKAN_CONTEXT_H
#define CORE_VULKAN_CONTEXT_H

// std
#include <cstdint>
#include <expected>
#include <span>

struct VkInstance_T;
struct VkDebugUtilsMessengerEXT_T;
struct VkSurfaceKHR_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VmaAllocator_T;
struct VkSwapchainKHR_T;

namespace core
{

struct VulkanContext {
public:
    static std::expected<VulkanContext, int32_t> createVulkanContext(std::span<const char*> requiredInstanceSurfaceExtensions,
                                                                     bool enableValidationLayersIfSupported,
                                                                     bool enableDebugMessengerIfSupported) noexcept;

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext(VulkanContext&&) noexcept;
    VulkanContext& operator=(VulkanContext&&) noexcept;

    ~VulkanContext() noexcept;

    VkInstance_T* instance;
    VkDebugUtilsMessengerEXT_T* debugMessenger;
    VkSurfaceKHR_T* surface;
    VkPhysicalDevice_T* physicalDevice;
    VkDevice_T* device;
    VmaAllocator_T* allocator;
    VkSwapchainKHR_T* swapchain;

private:
    VulkanContext(VkInstance_T* _instance,
                  VkDebugUtilsMessengerEXT_T* _debugMessenger,
                  VkSurfaceKHR_T* _surface,
                  VkPhysicalDevice_T* _physicalDevice,
                  VkDevice_T* _device,
                  VmaAllocator_T* _allocator,
                  VkSwapchainKHR_T* _swapchain) noexcept;
};

}   // namespace core

#endif
