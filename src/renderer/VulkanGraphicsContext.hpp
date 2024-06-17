#ifndef RENDERER_VULKAN_GRAPHICS_CONTEXT_H
#define RENDERER_VULKAN_GRAPHICS_CONTEXT_H

#include "core/UniqueVmaAllocator.hpp"

// libs
#include <vulkan/vulkan.hpp>

// std
#include <span>

struct SDL_Window;

namespace renderer
{

using PFN_presentModeKHRselector = vk::PresentModeKHR (*)(std::span<const vk::PresentModeKHR>);
using PFN_surfaceFormatKHRselector = vk::SurfaceFormatKHR (*)(std::span<const vk::SurfaceFormatKHR>);

struct VulkanGraphicsContextCreateInfo {
    VulkanGraphicsContextCreateInfo() noexcept;

    uint32_t vulkanApiVersion;

    std::span<const char* const> requiredInstanceExtensions;
    bool enableValidationLayersIfSupported;
    bool enableDebugMessengerIfSupported;

    SDL_Window* window;

    std::span<const char* const> requiredDeviceExtensions;
    vk::PhysicalDeviceFeatures* requiredDevice10Features;
    vk::PhysicalDeviceVulkan11Features* requiredDevice11Features;
    vk::PhysicalDeviceVulkan12Features* requiredDevice12Features;
    vk::PhysicalDeviceVulkan13Features* requiredDevice13Features;

    PFN_presentModeKHRselector pfnPresentModeKHRselector;
    PFN_surfaceFormatKHRselector pfnSurfaceFormatKHRselector;
};

class VulkanGraphicsContext
{
public:
    VulkanGraphicsContext() noexcept = default;
    VulkanGraphicsContext(const VulkanGraphicsContextCreateInfo& createInfo);

    VulkanGraphicsContext(const VulkanGraphicsContext&) = delete;
    VulkanGraphicsContext& operator=(const VulkanGraphicsContext&) = delete;

    VulkanGraphicsContext(VulkanGraphicsContext&&) noexcept = default;
    VulkanGraphicsContext& operator=(VulkanGraphicsContext&&) noexcept = default;

    ~VulkanGraphicsContext() noexcept = default;

public:
    vk::Device device() const noexcept { return m_device.get(); }

    uint32_t graphicsQueueFamilyIndex() const noexcept { return m_queueFamiliesIndices.graphicsFamilyIndex; }
    uint32_t presentQueueFamilyIndex() const noexcept { return m_queueFamiliesIndices.presentFamilyIndex; }
    uint32_t transferQueueFamilyIndex() const noexcept { return m_queueFamiliesIndices.transferFamilyIndex; }

    vk::Queue graphicsQueue() const noexcept { return m_queues.graphicsQueue; }
    vk::Queue presentQueue() const noexcept { return m_queues.graphicsQueue; }
    vk::Queue transferQueue() const noexcept { return m_queues.transferQueue; }

    VmaAllocator allocator() const noexcept { return m_allocator.get(); }

    vk::SwapchainKHR swapchain() const noexcept { return *m_swapchain; }
    vk::Image swapchainImage(uint32_t imageIndex) const noexcept { return m_swapchainImages[imageIndex]; }
    vk::ImageView swapchainImageView(uint32_t imageIndex) const noexcept { return *m_swapchainImageViews[imageIndex]; }
    vk::Format swapchainColorFormat() const noexcept { return m_currentSwapchainSurfaceFormat.format; }
    vk::Extent2D swapchainExtent() const noexcept { return m_currentSwapchainExtent; }

    void setSwapchainPresentMode(vk::PresentModeKHR presentModeKHR) noexcept
    {
        m_currentSwapchainPresentMode = presentModeKHR;
    }

    void setSwapchainSurfaceFormatKHR(const vk::SurfaceFormatKHR& surfaceFormatKHR) noexcept
    {
        m_currentSwapchainSurfaceFormat = surfaceFormatKHR;
    }

    void recreateSwapchain();

private:
    struct QueueFamiliesIndices {
        uint32_t graphicsFamilyIndex;
        uint32_t presentFamilyIndex;
        uint32_t transferFamilyIndex;
    };

    struct Queues {
        vk::Queue graphicsQueue;
        vk::Queue presentQueue;
        vk::Queue transferQueue;
    };

    void createInstanceAndDebug(uint32_t vulkanApiVersion,
                                std::span<const char* const> requiredInstanceExtensions,
                                bool enableValidationLayersIfSupported,
                                bool enableDebugMessengerIfSupported);

    void searchPhysicalDevice(std::span<const char* const> requiredDeviceExtensions,
                              const vk::PhysicalDeviceFeatures* requiredDevice10Features,
                              const vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                              const vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                              const vk::PhysicalDeviceVulkan13Features* requiredDevice13Features);

    void createLogicalDevice(std::span<const char* const> requiredDeviceExtensions,
                             vk::PhysicalDeviceFeatures* requiredDevice10Features,
                             vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                             vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                             vk::PhysicalDeviceVulkan13Features* requiredDevice13Features);

    void createAllocator(uint32_t vulkanApiVersion, bool useBufferDeviceAddressFeature);

    void createSwapchain(PFN_presentModeKHRselector pfnPresentModeKHRselector,
                         PFN_surfaceFormatKHRselector pfnSurfaceFormatKHRselector);

private:
    SDL_Window* m_window;   // not owned
    vk::UniqueInstance m_instance;
    vk::UniqueDebugUtilsMessengerEXT m_debugMessenger;
    vk::UniqueSurfaceKHR m_surface;
    vk::PhysicalDevice m_physicalDevice;
    QueueFamiliesIndices m_queueFamiliesIndices;
    vk::UniqueDevice m_device;
    Queues m_queues;
    core::UniqueVmaAllocator m_allocator;
    vk::PresentModeKHR m_currentSwapchainPresentMode;
    vk::SurfaceFormatKHR m_currentSwapchainSurfaceFormat;
    vk::Extent2D m_currentSwapchainExtent;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<vk::Image> m_swapchainImages;
    std::vector<vk::UniqueImageView> m_swapchainImageViews;
};

}   // namespace renderer

#endif
