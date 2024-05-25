#include "VulkanContext.hpp"

// libs
#define VK_NO_PROTOTYPES
#include <vk_mem_alloc.h>
#include <volk.h>

// std
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <new>
#include <utility>
#include <vector>

// Because std still does not support std::expected in other types,
// such as std::vector, there are internal "try-catches" that converts
// std::bad_alloc to VK_ERROR_OUT_OF_HOST_MEMORY

namespace core
{

namespace
{

VkBool32 debugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT messageType,
                                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void* pUserData) noexcept
{
    (void) messageSeverity;
    (void) messageType;
    (void) pUserData;

    std::cout << "validation layer: " << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}

void cleanup(VkInstance instance,
             VkDebugUtilsMessengerEXT debugMessenger = nullptr,
             VkSurfaceKHR surface = nullptr,
             VkDevice device = nullptr,
             VmaAllocator allocator = nullptr,
             VkSwapchainKHR swapchain = nullptr) noexcept
{
    if (debugMessenger) {
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);
}

std::expected<bool, int32_t> checkValidationLayersSupport() noexcept
{
    try {
        uint32_t propertyCount;
        VkResult vkRes = vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
        if (vkRes != VK_SUCCESS) {
            return std::unexpected(vkRes);
        }
        std::vector<VkLayerProperties> instanceLayerProperties(propertyCount);   // may throw
        vkRes = vkEnumerateInstanceLayerProperties(&propertyCount, instanceLayerProperties.data());
        if (vkRes != VK_SUCCESS) {
            return std::unexpected(vkRes);
        }

        auto match = [](const VkLayerProperties& p) {
            return std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        };

        bool supportsValidationLayers = false;
        if (auto result = std::ranges::find_if(instanceLayerProperties, match);
            result != instanceLayerProperties.end()) {
            supportsValidationLayers = true;
        }
        return supportsValidationLayers;
    } catch (std::bad_alloc& e) {
        return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);
    }
}

std::expected<bool, uint32_t> checkDebugMessengerSupport() noexcept
{
    try {
        uint32_t propertyCount;
        VkResult vkRes = vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
        if (vkRes != VK_SUCCESS) {
            return std::unexpected(vkRes);
        }

        std::vector<VkExtensionProperties> instanceExtensionProperties(propertyCount);   // may throw
        vkRes = vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, instanceExtensionProperties.data());
        if (vkRes != VK_SUCCESS) {
            return std::unexpected(vkRes);
        }

        auto match = [](const VkExtensionProperties& p) {
            return std::strcmp(p.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
        };

        bool supportsDebugMessenger = false;
        if (auto result = std::ranges::find_if(instanceExtensionProperties, match);
            result != instanceExtensionProperties.end()) {
            supportsDebugMessenger = true;
        }
        return supportsDebugMessenger;
    } catch (std::bad_alloc& e) {
        return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);
    }
}

std::expected<std::pair<VkInstance, VkDebugUtilsMessengerEXT>, int32_t>
    createInstanceAndDebug(std::span<const char*> requiredInstanceExtensions,
                           bool enableValidationLayersIfSupported,
                           bool enableDebugMessengerIfSupported) noexcept
{
    VkResult vkRes = volkInitialize();
    if (vkRes != VK_SUCCESS) {
        return std::unexpected(vkRes);
    }

    VkApplicationInfo applicationInfo {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                       .pNext = nullptr,
                                       .pApplicationName = "Voxel game",
                                       .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
                                       .pEngineName = "No engine",
                                       .engineVersion = VK_API_VERSION_1_0,
                                       .apiVersion = VK_API_VERSION_1_3};

    bool useValidationLayers = false;
    if (enableValidationLayersIfSupported) {
        auto res = checkValidationLayersSupport();
        if (!res.has_value()) {
            return std::unexpected(res.error());
        }
        useValidationLayers = res.value();
    }

    bool useDebugMessenger = false;
    if (enableDebugMessengerIfSupported) {
        auto res = checkDebugMessengerSupport();
        if (!res.has_value()) {
            return std::unexpected(res.error());
        }
        useDebugMessenger = res.value();
    }

    uint32_t layerCount = useValidationLayers ? 1 : 0;
    const char* pLayer = useValidationLayers ? "VK_LAYER_KHRONOS_validation" : nullptr;

    try {
        // may throw
        std::vector<const char*> extensions(requiredInstanceExtensions.begin(), requiredInstanceExtensions.end());
        if (useDebugMessenger) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugMessengerCallback,
            .pUserData = nullptr};

        void* instancePNext = useDebugMessenger ? &debugMessengerCreateInfo : nullptr;

        VkInstanceCreateInfo instanceCreateInfo {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                 .pNext = instancePNext,
                                                 .flags = 0,
                                                 .pApplicationInfo = &applicationInfo,
                                                 .enabledLayerCount = layerCount,
                                                 .ppEnabledLayerNames = &pLayer,
                                                 .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                                 .ppEnabledExtensionNames = extensions.data()};

        VkInstance instance;
        vkRes = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
        if (vkRes != VK_SUCCESS) {
            return std::unexpected(vkRes);
        }
        volkLoadInstance(instance);

        VkDebugUtilsMessengerEXT debugMessenger;
        if (useDebugMessenger) {
            vkRes = vkCreateDebugUtilsMessengerEXT(instance, &debugMessengerCreateInfo, nullptr, &debugMessenger);
            if (vkRes != VK_SUCCESS) {
                cleanup(instance);
                return std::unexpected(vkRes);
            }
        } else {
            debugMessenger = nullptr;
        }
        return std::pair<VkInstance, VkDebugUtilsMessengerEXT>(instance, debugMessenger);
    } catch (std::bad_alloc& e) {
        return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);
    }
}

}   // namespace

std::expected<VulkanContext, int32_t>
    VulkanContext::createVulkanContext(std::span<const char*> requiredInstanceExtensions,
                                       bool enableValidationLayersIfSupported,
                                       bool enableDebugMessengerIfSupported) noexcept
{
    auto instanceDebugRes = createInstanceAndDebug(requiredInstanceExtensions,
                                                   enableValidationLayersIfSupported,
                                                   enableDebugMessengerIfSupported);
    if (!instanceDebugRes.has_value()) {
        return std::unexpected(instanceDebugRes.error());
    }

    return VulkanContext(instanceDebugRes->first,
                         instanceDebugRes->second,
                         nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         nullptr);
}

VulkanContext::VulkanContext(VkInstance_T* _instance,
                             VkDebugUtilsMessengerEXT_T* _debugMessenger,
                             VkSurfaceKHR_T* _surface,
                             VkPhysicalDevice_T* _physicalDevice,
                             VkDevice_T* _device,
                             VmaAllocator_T* _allocator,
                             VkSwapchainKHR_T* _swapchain) noexcept
    : instance(_instance)
    , debugMessenger(_debugMessenger)
    , surface(_surface)
    , physicalDevice(_physicalDevice)
    , device(_device)
    , allocator(_allocator)
    , swapchain(_swapchain)
{
    assert(instance);
    // assert(debugMessenger); // Allowed, can be nullptr if debug is disabled / not supported

    // Not implemented yet
    // assert(surface);
    // assert(physicalDevice);
    // assert(device);
    // assert(allocator);
    // assert(swapchain);
}

VulkanContext::VulkanContext(VulkanContext&& rhs) noexcept
    : instance(std::exchange(rhs.instance, nullptr))
    , debugMessenger(std::exchange(rhs.debugMessenger, nullptr))
    , surface(std::exchange(rhs.surface, nullptr))
    , physicalDevice(std::exchange(rhs.physicalDevice, nullptr))
    , device(std::exchange(rhs.device, nullptr))
    , allocator(std::exchange(rhs.allocator, nullptr))
    , swapchain(std::exchange(rhs.swapchain, nullptr))
{}

VulkanContext& VulkanContext::operator=(VulkanContext&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(instance, rhs.instance);
        std::swap(debugMessenger, rhs.debugMessenger);
        std::swap(surface, rhs.surface);
        std::swap(physicalDevice, rhs.physicalDevice);
        std::swap(device, rhs.device);
        std::swap(allocator, rhs.allocator);
        std::swap(swapchain, rhs.swapchain);
    }
    return *this;
}

VulkanContext::~VulkanContext() noexcept
{
    cleanup(instance, debugMessenger, surface, device, allocator, swapchain);
}

}   // namespace core
