#define VMA_IMPLEMENTATION

#include "VulkanGraphicsContext.hpp"

#include "Utils.hpp"
#include "core/Logger.hpp"

// libs
#include <SDL_events.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan_to_string.hpp>

// std
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace renderer
{

namespace
{

VkBool32 debugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT messageType,
                                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void* pUserData) noexcept
{
    (void) messageType;
    (void) pUserData;

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            ERROR_FMT("[VALIDATION LAYER]: {}\n", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            WARN_FMT("[VALIDATION LAYER]: {}\n", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            INFO_FMT("[VALIDATION LAYER]: {}\n", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        default: TRACE_FMT("[VALIDATION LAYER]: {}\n", pCallbackData->pMessage); break;
    }

    return VK_FALSE;
}

bool isValidationLayerSupported()
{
    auto instanceLayerProperties = vk::enumerateInstanceLayerProperties();

    auto match = [](const vk::LayerProperties& p) {
        return std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0;
    };

    bool supportsValidationLayers = false;
    if (auto result = std::ranges::find_if(instanceLayerProperties, match); result != instanceLayerProperties.end()) {
        supportsValidationLayers = true;
    }
    return supportsValidationLayers;
}

}   // namespace

VulkanGraphicsContextCreateInfo::VulkanGraphicsContextCreateInfo() noexcept
    : vulkanApiVersion(0)
    , requiredInstanceExtensions()
    , enableValidationLayersIfSupported(false)
    , enableDebugMessengerIfSupported(false)
    , window(nullptr)
    , requiredDeviceExtensions()
    , requiredDevice10Features(nullptr)
    , requiredDevice11Features(nullptr)
    , requiredDevice12Features(nullptr)
    , requiredDevice13Features(nullptr)
{}

VulkanGraphicsContext::VulkanGraphicsContext(const VulkanGraphicsContextCreateInfo& createInfo)
{
    assert(createInfo.vulkanApiVersion >= vk::ApiVersion10);
    assert(createInfo.window);
    assert(utils::containsExtension(createInfo.requiredDeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME));

    m_window = createInfo.window;

    VULKAN_HPP_DEFAULT_DISPATCHER.init();   // default dispatcher is noexcept
    {
        // local scope to not misuse instanceVersion with vulkanApiVersion
        auto instanceVersion = VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumerateInstanceVersion ? vk::enumerateInstanceVersion()
                                                                                        : vk::ApiVersion10;
        DEBUG_FMT("Machine Vulkan API version: p{} {}.{}.{}\n",
                  vk::apiVersionVariant(instanceVersion),
                  vk::apiVersionMajor(instanceVersion),
                  vk::apiVersionMinor(instanceVersion),
                  vk::apiVersionPatch(instanceVersion));

        if (instanceVersion < createInfo.vulkanApiVersion) {
            auto errorMsg = std::format("The machine cannot support the application needs!\nThe minimum required "
                                        "version specified is v{} {}.{}.{}",
                                        vk::apiVersionVariant(createInfo.vulkanApiVersion),
                                        vk::apiVersionMajor(createInfo.vulkanApiVersion),
                                        vk::apiVersionMinor(createInfo.vulkanApiVersion),
                                        vk::apiVersionPatch(createInfo.vulkanApiVersion));

            FATAL_FMT("{}\n", errorMsg);
            throw vk::IncompatibleDriverError(errorMsg);
        }
    }

    createInstanceAndDebug(createInfo.vulkanApiVersion,
                           createInfo.requiredInstanceExtensions,
                           createInfo.enableValidationLayersIfSupported,
                           createInfo.enableDebugMessengerIfSupported);

    {
        VkSurfaceKHR tempSurface;
        SDL_bool SDLRes = SDL_Vulkan_CreateSurface(createInfo.window, *m_instance, &tempSurface);
        if (SDLRes != SDL_TRUE) {
            // To be consistent with vulkan.hpp, throw a vk::SystemError
            throw vk::UnknownError(SDL_GetError());
        }
        m_surface = vk::UniqueSurfaceKHR(tempSurface, *m_instance);
        DEBUG("Successfully created surface\n");
    }

    searchPhysicalDevice(createInfo.requiredDeviceExtensions,
                         createInfo.requiredDevice10Features,
                         createInfo.requiredDevice11Features,
                         createInfo.requiredDevice12Features,
                         createInfo.requiredDevice13Features);

    createLogicalDevice(createInfo.requiredDeviceExtensions,
                        createInfo.requiredDevice10Features,
                        createInfo.requiredDevice11Features,
                        createInfo.requiredDevice12Features,
                        createInfo.requiredDevice13Features);

    createAllocator(createInfo.vulkanApiVersion,
                    createInfo.requiredDevice12Features && createInfo.requiredDevice12Features->bufferDeviceAddress);

    createSwapchain();

    assert(m_instance);
    // assert(m_debugMessenger); // Allowed, can be nullptr if debug is disabled / not supported
    assert(m_surface);
    assert(m_physicalDevice);
    assert(m_device);
    assert(m_queues.graphicsQueue);
    assert(m_queues.presentQueue);
    assert(m_queues.transferQueue);
    assert(m_allocator);
    assert(m_swapchain);
}

void VulkanGraphicsContext::createInstanceAndDebug(uint32_t vulkanApiVersion,
                                                   std::span<const char* const> requiredInstanceExtensions,
                                                   bool enableValidationLayersIfSupported,
                                                   bool enableDebugMessengerIfSupported)
{
    vk::ApplicationInfo applicationInfo {.sType = vk::StructureType::eApplicationInfo,
                                         .pNext = nullptr,
                                         .pApplicationName = nullptr,
                                         .applicationVersion = vk::makeApiVersion(0, 0, 1, 0),
                                         .pEngineName = nullptr,
                                         .engineVersion = 0,
                                         .apiVersion = vulkanApiVersion};

    bool useValidationLayers = enableValidationLayersIfSupported && isValidationLayerSupported();
    if (enableValidationLayersIfSupported && !useValidationLayers) {
        INFO("Validation layer requested, but not supported\n");
    }

    auto availableInstanceExtensions = vk::enumerateInstanceExtensionProperties();
    bool useDebugMessenger = enableDebugMessengerIfSupported &&
                             utils::containsExtension(availableInstanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (enableDebugMessengerIfSupported && !useDebugMessenger) {
        INFO("Debug messenger util extension requested, but not supported\n");
    }

    uint32_t layerCount = useValidationLayers ? 1 : 0;
    const char* pLayer = useValidationLayers ? "VK_LAYER_KHRONOS_validation" : nullptr;

    std::vector<const char*> extensions(requiredInstanceExtensions.begin(), requiredInstanceExtensions.end());
    if (useDebugMessenger) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
        .sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT,
        .pNext = nullptr,
        .flags = {},
        .messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugMessengerCallback,
        .pUserData = nullptr};

    void* instancePNext = useDebugMessenger ? &debugMessengerCreateInfo : nullptr;

    vk::InstanceCreateInfo instanceCreateInfo {.sType = vk::StructureType::eInstanceCreateInfo,
                                               .pNext = instancePNext,
                                               .flags = {},
                                               .pApplicationInfo = &applicationInfo,
                                               .enabledLayerCount = layerCount,
                                               .ppEnabledLayerNames = &pLayer,
                                               .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                               .ppEnabledExtensionNames = extensions.data()};

    m_instance = vk::createInstanceUnique(instanceCreateInfo);
    DEBUG("Successfully created instance\n");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_instance);

    if (useDebugMessenger) {
        m_debugMessenger = m_instance->createDebugUtilsMessengerEXTUnique(debugMessengerCreateInfo);
        DEBUG("Successfully created debug messenger\n");
    }
}

static bool supportsRequiredDeviceFeatures(vk::PhysicalDevice physicalDevice,
                                           const vk::PhysicalDeviceFeatures* requiredDevice10Features,
                                           const vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                                           const vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                                           const vk::PhysicalDeviceVulkan13Features* requiredDevice13Features);

void VulkanGraphicsContext::searchPhysicalDevice(std::span<const char* const> requiredDeviceExtensions,
                                                 const vk::PhysicalDeviceFeatures* requiredDevice10Features,
                                                 const vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                                                 const vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                                                 const vk::PhysicalDeviceVulkan13Features* requiredDevice13Features)
{
    // TODO: improve physical device selection.
    // Currently selecting the first one with graphicsQueueFamily + presentQueueFamily + swapchainSupport

    auto physicalDevices = m_instance->enumeratePhysicalDevices();

    for (const auto& pd : physicalDevices) {
        auto pdProperties = pd.getProperties();
        DEBUG_FMT("Encountered physical device: {}\n", static_cast<const char*>(pdProperties.deviceName));
    }

    for (const auto& pd : physicalDevices) {
        auto queueFamiliesProperties = pd.getQueueFamilyProperties();

        std::optional<uint32_t> graphicsFamilyIndex;
        std::optional<uint32_t> presentFamilyIndex;
        std::optional<uint32_t> transferFamilyIndex;

        for (size_t i = 0; i < queueFamiliesProperties.size(); ++i) {
            const auto& qp = queueFamiliesProperties[i];
            if (qp.queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsFamilyIndex = i;
                transferFamilyIndex = i;
            }

            if (pd.getSurfaceSupportKHR(i, *m_surface)) {
                presentFamilyIndex = i;
            }

            // if (qp.queueFlags & vk::QueueFlagBits::eTransfer &&
            //     !(qp.queueFlags & vk::QueueFlagBits::eGraphics && !(qp.queueFlags & vk::QueueFlagBits::eCompute))) {
            //     transferFamilyIndex = i;
            // }

            if (graphicsFamilyIndex && presentFamilyIndex && transferFamilyIndex) {
                break;
            }
        }

        auto availablePhysicalDeviceExtensions = pd.enumerateDeviceExtensionProperties();
        bool supportsAllExtensions = true;
        for (auto ext : requiredDeviceExtensions) {
            if (!utils::containsExtension(availablePhysicalDeviceExtensions, ext)) {
                supportsAllExtensions = false;
                break;
            }
        }

        bool supportsAllFeatures = supportsRequiredDeviceFeatures(pd,
                                                                  requiredDevice10Features,
                                                                  requiredDevice11Features,
                                                                  requiredDevice12Features,
                                                                  requiredDevice13Features);

        if (graphicsFamilyIndex && presentFamilyIndex && transferFamilyIndex && supportsAllExtensions &&
            supportsAllFeatures) {
            auto pdProperties = pd.getProperties();
            DEBUG_FMT(
                "Successfully encountered a suitable physical device.\nName: {}\nApi version: v{} {}.{}.{}\nDriver "
                "version: {}\n",
                static_cast<const char*>(pdProperties.deviceName),
                vk::apiVersionVariant(pdProperties.apiVersion),
                vk::apiVersionMajor(pdProperties.apiVersion),
                vk::apiVersionMinor(pdProperties.apiVersion),
                vk::apiVersionPatch(pdProperties.apiVersion),
                pdProperties.driverVersion);

            m_physicalDevice = pd;
            m_queueFamiliesIndices.graphicsFamilyIndex = *graphicsFamilyIndex;
            m_queueFamiliesIndices.presentFamilyIndex = *presentFamilyIndex;
            m_queueFamiliesIndices.transferFamilyIndex = *transferFamilyIndex;
            DEBUG_FMT("Queue families indices: graphics {}, present {}, transfer {}\n",
                      m_queueFamiliesIndices.graphicsFamilyIndex,
                      m_queueFamiliesIndices.presentFamilyIndex,
                      m_queueFamiliesIndices.transferFamilyIndex);

            return;
        }
    }
    // To be consistent with vulkan.hpp, throw a vk::SystemError
    throw vk::UnknownError("No physical device matched the application requirements");
}

void VulkanGraphicsContext::createLogicalDevice(std::span<const char* const> requiredDeviceExtensions,
                                                vk::PhysicalDeviceFeatures* requiredDevice10Features,
                                                vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                                                vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                                                vk::PhysicalDeviceVulkan13Features* requiredDevice13Features)
{
    std::vector<uint32_t> uniqueQueueFamiliesIndices {m_queueFamiliesIndices.graphicsFamilyIndex,
                                                      m_queueFamiliesIndices.presentFamilyIndex,
                                                      m_queueFamiliesIndices.transferFamilyIndex};
    // remove duplicate elements
    uniqueQueueFamiliesIndices.erase(std::unique(uniqueQueueFamiliesIndices.begin(), uniqueQueueFamiliesIndices.end()),
                                     uniqueQueueFamiliesIndices.end());

    float queuePriorities[] {1.f};
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamiliesIndices.size());

    std::transform(uniqueQueueFamiliesIndices.begin(),
                   uniqueQueueFamiliesIndices.end(),
                   std::back_inserter(queueCreateInfos),
                   [&queuePriorities](uint32_t queueFamilyIndex) {
                       return vk::DeviceQueueCreateInfo {.sType = vk::StructureType::eDeviceQueueCreateInfo,
                                                         .pNext = nullptr,
                                                         .flags = {},
                                                         .queueFamilyIndex = queueFamilyIndex,
                                                         .queueCount = std::size(queuePriorities),
                                                         .pQueuePriorities = queuePriorities};
                   });

    // Create a structure chain of device features
    // 1.0 features is directly passed as pFeatures
    void* deviceCreateInfoPNext = nullptr;   // none

    if (requiredDevice11Features) {
        // Emplace 1.1 in the front of the chain
        deviceCreateInfoPNext = requiredDevice11Features;
    }

    if (requiredDevice12Features) {
        if (requiredDevice11Features && deviceCreateInfoPNext == requiredDevice11Features) {
            // 1.1 is the beggining of the chain.
            requiredDevice12Features->pNext = requiredDevice11Features;
        }
        // Emplace 1.2 in the front of the chain
        deviceCreateInfoPNext = requiredDevice12Features;
    }

    if (requiredDevice13Features) {
        if (requiredDevice11Features && deviceCreateInfoPNext == requiredDevice11Features) {
            // 1.1 is the beggining of the chain.
            requiredDevice13Features->pNext = requiredDevice11Features;
        } else if (requiredDevice12Features && deviceCreateInfoPNext == requiredDevice12Features) {
            // 1.2 is the beggining of the chain.
            requiredDevice13Features->pNext = requiredDevice12Features;
        }
        // Emplace 1.3 in the front of the chain
        deviceCreateInfoPNext = requiredDevice13Features;
    }

    vk::DeviceCreateInfo deviceCreateInfo {.sType = vk::StructureType::eDeviceCreateInfo,
                                           .pNext = deviceCreateInfoPNext,
                                           .flags = {},
                                           .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                           .pQueueCreateInfos = queueCreateInfos.data(),
                                           .enabledLayerCount = 0,
                                           .ppEnabledLayerNames = nullptr,
                                           .enabledExtensionCount =
                                               static_cast<uint32_t>(requiredDeviceExtensions.size()),
                                           .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
                                           .pEnabledFeatures = requiredDevice10Features};

    m_device = m_physicalDevice.createDeviceUnique(deviceCreateInfo);
    DEBUG("Successfully created logical device\n");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_device);

    m_queues.graphicsQueue = m_device->getQueue(m_queueFamiliesIndices.graphicsFamilyIndex, 0);
    m_queues.presentQueue = m_device->getQueue(m_queueFamiliesIndices.presentFamilyIndex, 0);
    m_queues.transferQueue = m_device->getQueue(m_queueFamiliesIndices.transferFamilyIndex, 0);
}

void VulkanGraphicsContext::createAllocator(uint32_t vulkanApiVersion, bool useBufferDeviceAddressFeature)
{
    const auto& d = VULKAN_HPP_DEFAULT_DISPATCHER;
    // Let VMA handle the function pointers loading
    VmaVulkanFunctions vulkanFunctions {};
    vulkanFunctions.vkGetInstanceProcAddr = d.vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = d.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo {.flags = useBufferDeviceAddressFeature
                                                             ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
                                                             : static_cast<VmaAllocatorCreateFlags>(0),
                                                .physicalDevice = m_physicalDevice,
                                                .device = *m_device,
                                                .preferredLargeHeapBlockSize = 0,
                                                .pAllocationCallbacks = nullptr,
                                                .pDeviceMemoryCallbacks = nullptr,
                                                .pHeapSizeLimit = nullptr,
                                                .pVulkanFunctions = &vulkanFunctions,
                                                .instance = *m_instance,
                                                .vulkanApiVersion = vulkanApiVersion,
                                                .pTypeExternalMemoryHandleTypes = nullptr};

    m_allocator = core::UniqueVmaAllocator(allocatorCreateInfo);
    DEBUG("Successfully created vmaAllocator\n");
}

void VulkanGraphicsContext::createSwapchain()
{
    // The Vulkan specs requires VK_KHR_surface extension to support at least VK_PRESENT_MODE_FIFO_KHR present mode
    // (ref VkPresentModeKHR(3) Manual Page)
    // and requires at least one VkSurfaceFormatKHR to be supported, with format != undefined
    // (ref vkGetPhysicalDeviceSurfaceFormatsKHR(3) Manual Page).

    m_currentSwapchainPresentMode = vk::PresentModeKHR::eFifo;

    auto surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(*m_surface);
    assert(surfaceFormats.size() > 0);
    m_currentSwapchainSurfaceFormat = surfaceFormats[0];

    DEBUG_FMT("Using swapchain present mode = {}, swapchain surface format = {{{}, {}}}\n",
              vk::to_string(m_currentSwapchainPresentMode),
              vk::to_string(m_currentSwapchainSurfaceFormat.format),
              vk::to_string(m_currentSwapchainSurfaceFormat.colorSpace));

    recreateSwapchain();
}

void VulkanGraphicsContext::recreateSwapchain()
{
    auto surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);

    vk::Extent2D newSwapchainExtent;
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        newSwapchainExtent = surfaceCapabilities.currentExtent;
    } else {
        int w, h;
        SDL_Vulkan_GetDrawableSize(m_window, &w, &h);
        // handle minimization
        while (w == 0 || h == 0) {
            SDL_Vulkan_GetDrawableSize(m_window, &w, &h);
            SDL_WaitEvent(nullptr);
        }

        vk::Extent2D actualExtent(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        actualExtent.width = std::clamp(actualExtent.width,
                                        surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width);

        actualExtent.height = std::clamp(actualExtent.height,
                                         surfaceCapabilities.minImageExtent.height,
                                         surfaceCapabilities.maxImageExtent.height);
        newSwapchainExtent = actualExtent;
    }

    uint32_t imageCount = surfaceCapabilities.maxImageCount == 0   // unlimited
                              ? surfaceCapabilities.minImageCount + 1
                              : std::min(surfaceCapabilities.maxImageCount, surfaceCapabilities.minImageCount + 1);

    vk::SharingMode imageSharingMode;
    uint32_t queueFamilyIndexCount;
    uint32_t* pQueueFamilyIndices;
    // array of queue family indices having access to the images(s) of the swapchain when imageSharingMode is
    // VK_SHARING_MODE_CONCURRENT
    uint32_t queueFamilyIndices[] = {m_queueFamiliesIndices.graphicsFamilyIndex,
                                     m_queueFamiliesIndices.presentFamilyIndex};

    // TODO: these probably can be stored and be immutable, but is it worth it?
    if (m_queueFamiliesIndices.graphicsFamilyIndex != m_queueFamiliesIndices.presentFamilyIndex) {
        imageSharingMode = vk::SharingMode::eConcurrent;
        queueFamilyIndexCount = std::size(queueFamilyIndices);
        pQueueFamilyIndices = queueFamilyIndices;
    } else {
        imageSharingMode = vk::SharingMode::eExclusive;
        queueFamilyIndexCount = 0;
        pQueueFamilyIndices = nullptr;
    }

    // on the first call, this will create the swapchain, as m_swapchain is initialized as nullptr
    // otherwise pass it as old swapchain
    vk::SwapchainKHR oldSwapchain = *m_swapchain;
    vk::SwapchainCreateInfoKHR swapchainCreateInfo {.sType = vk::StructureType::eSwapchainCreateInfoKHR,
                                                    .pNext = nullptr,
                                                    .flags = {},
                                                    .surface = *m_surface,
                                                    .minImageCount = imageCount,
                                                    .imageFormat = m_currentSwapchainSurfaceFormat.format,
                                                    .imageColorSpace = m_currentSwapchainSurfaceFormat.colorSpace,
                                                    .imageExtent = newSwapchainExtent,
                                                    .imageArrayLayers = 1,
                                                    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                                                    .imageSharingMode = imageSharingMode,
                                                    .queueFamilyIndexCount = queueFamilyIndexCount,
                                                    .pQueueFamilyIndices = pQueueFamilyIndices,
                                                    .preTransform = surfaceCapabilities.currentTransform,
                                                    .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                    .presentMode = m_currentSwapchainPresentMode,
                                                    .clipped = vk::True,
                                                    .oldSwapchain = oldSwapchain};

    // We could keep the retired old swapchain to try to present already retrieved images, but is this worth it?
    // in which scenarios would need a Swapchain recreation AND have the oldSwapchain NOT entered a state that
    // causes VK_ERROR_OUT_OF_DATE_KHR to be returned?
    // furthermore, does applications frequently acquire more than 1 image before presenting it?
    // Unique Handle will destroy it

    m_swapchain = m_device->createSwapchainKHRUnique(swapchainCreateInfo);
    TRACE_FMT("Successfully {} swapchain\n", oldSwapchain == nullptr ? "created" : "re-created");
    m_currentSwapchainExtent = newSwapchainExtent;

    m_swapchainImages = m_device->getSwapchainImagesKHR(*m_swapchain);
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
        vk::ImageViewCreateInfo imageViewCreateInfo {
            .sType = vk::StructureType::eImageViewCreateInfo,
            .pNext = nullptr,
            .flags = {},
            .image = m_swapchainImages[i],
            .viewType = vk::ImageViewType::e2D,
            .format = m_currentSwapchainSurfaceFormat.format,
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
        m_swapchainImageViews[i] = m_device->createImageViewUnique(imageViewCreateInfo);
    }
    TRACE("Successfully retrieved swapchain image views\n");
}

static bool supportsRequiredDeviceFeatures(vk::PhysicalDevice physicalDevice,
                                           const vk::PhysicalDeviceFeatures* requiredDevice10Features,
                                           const vk::PhysicalDeviceVulkan11Features* requiredDevice11Features,
                                           const vk::PhysicalDeviceVulkan12Features* requiredDevice12Features,
                                           const vk::PhysicalDeviceVulkan13Features* requiredDevice13Features)
{
    auto deviceApiVersion = physicalDevice.getProperties().apiVersion;

    vk::PhysicalDeviceFeatures2 chain;
    auto& supported10Features = chain.features;
    vk::PhysicalDeviceVulkan11Features supported11Features;
    vk::PhysicalDeviceVulkan12Features supported12Features;
    vk::PhysicalDeviceVulkan13Features supported13Features;

    if (deviceApiVersion >= vk::ApiVersion11) {
        chain.pNext = &supported11Features;
        if (deviceApiVersion >= vk::ApiVersion12) {
            supported11Features.pNext = &supported12Features;
            if (deviceApiVersion >= vk::ApiVersion13) {
                supported12Features.pNext = &supported13Features;
            }
        }
    }

    physicalDevice.getFeatures2(&chain);

    bool supportsAllFeatures = true;
    if (requiredDevice10Features) {
        // clang-format off
        if ((requiredDevice10Features->robustBufferAccess && !supported10Features.robustBufferAccess) ||
        (requiredDevice10Features->fullDrawIndexUint32 && !supported10Features.fullDrawIndexUint32) ||
        (requiredDevice10Features->imageCubeArray && !supported10Features.imageCubeArray) ||
        (requiredDevice10Features->independentBlend && !supported10Features.independentBlend) ||
        (requiredDevice10Features->geometryShader && !supported10Features.geometryShader) ||
        (requiredDevice10Features->tessellationShader && !supported10Features.tessellationShader) ||
        (requiredDevice10Features->sampleRateShading && !supported10Features.sampleRateShading) ||
        (requiredDevice10Features->dualSrcBlend && !supported10Features.dualSrcBlend) ||
        (requiredDevice10Features->logicOp && !supported10Features.logicOp) ||
        (requiredDevice10Features->multiDrawIndirect && !supported10Features.multiDrawIndirect) ||
        (requiredDevice10Features->drawIndirectFirstInstance && !supported10Features.drawIndirectFirstInstance) ||
        (requiredDevice10Features->depthClamp && !supported10Features.depthClamp) ||
        (requiredDevice10Features->depthBiasClamp && !supported10Features.depthBiasClamp) ||
        (requiredDevice10Features->fillModeNonSolid && !supported10Features.fillModeNonSolid) ||
        (requiredDevice10Features->depthBounds && !supported10Features.depthBounds) ||
        (requiredDevice10Features->wideLines && !supported10Features.wideLines) ||
        (requiredDevice10Features->largePoints && !supported10Features.largePoints) ||
        (requiredDevice10Features->alphaToOne && !supported10Features.alphaToOne) ||
        (requiredDevice10Features->multiViewport && !supported10Features.multiViewport) ||
        (requiredDevice10Features->samplerAnisotropy && !supported10Features.samplerAnisotropy) ||
        (requiredDevice10Features->textureCompressionETC2 && !supported10Features.textureCompressionETC2) ||
        (requiredDevice10Features->textureCompressionASTC_LDR && !supported10Features.textureCompressionASTC_LDR) ||
        (requiredDevice10Features->textureCompressionBC && !supported10Features.textureCompressionBC) ||
        (requiredDevice10Features->occlusionQueryPrecise && !supported10Features.occlusionQueryPrecise) ||
        (requiredDevice10Features->pipelineStatisticsQuery && !supported10Features.pipelineStatisticsQuery) ||
        (requiredDevice10Features->vertexPipelineStoresAndAtomics && !supported10Features.vertexPipelineStoresAndAtomics) ||
        (requiredDevice10Features->fragmentStoresAndAtomics && !supported10Features.fragmentStoresAndAtomics) ||
        (requiredDevice10Features->shaderTessellationAndGeometryPointSize && !supported10Features.shaderTessellationAndGeometryPointSize) ||
        (requiredDevice10Features->shaderImageGatherExtended && !supported10Features.shaderImageGatherExtended) ||
        (requiredDevice10Features->shaderStorageImageExtendedFormats && !supported10Features.shaderStorageImageExtendedFormats) ||
        (requiredDevice10Features->shaderStorageImageMultisample && !supported10Features.shaderStorageImageMultisample) ||
        (requiredDevice10Features->shaderStorageImageReadWithoutFormat && !supported10Features.shaderStorageImageReadWithoutFormat) ||
        (requiredDevice10Features->shaderStorageImageWriteWithoutFormat && !supported10Features.shaderStorageImageWriteWithoutFormat) ||
        (requiredDevice10Features->shaderUniformBufferArrayDynamicIndexing && !supported10Features.shaderUniformBufferArrayDynamicIndexing) ||
        (requiredDevice10Features->shaderSampledImageArrayDynamicIndexing && !supported10Features.shaderSampledImageArrayDynamicIndexing) ||
        (requiredDevice10Features->shaderStorageBufferArrayDynamicIndexing && !supported10Features.shaderStorageBufferArrayDynamicIndexing) ||
        (requiredDevice10Features->shaderStorageImageArrayDynamicIndexing && !supported10Features.shaderStorageImageArrayDynamicIndexing) ||
        (requiredDevice10Features->shaderClipDistance && !supported10Features.shaderClipDistance) ||
        (requiredDevice10Features->shaderCullDistance && !supported10Features.shaderCullDistance) ||
        (requiredDevice10Features->shaderFloat64 && !supported10Features.shaderFloat64) ||
        (requiredDevice10Features->shaderInt64 && !supported10Features.shaderInt64) ||
        (requiredDevice10Features->shaderInt16 && !supported10Features.shaderInt16) ||
        (requiredDevice10Features->shaderResourceResidency && !supported10Features.shaderResourceResidency) ||
        (requiredDevice10Features->shaderResourceMinLod && !supported10Features.shaderResourceMinLod) ||
        (requiredDevice10Features->sparseBinding && !supported10Features.sparseBinding) ||
        (requiredDevice10Features->sparseResidencyBuffer && !supported10Features.sparseResidencyBuffer) ||
        (requiredDevice10Features->sparseResidencyImage2D && !supported10Features.sparseResidencyImage2D) ||
        (requiredDevice10Features->sparseResidencyImage3D && !supported10Features.sparseResidencyImage3D) ||
        (requiredDevice10Features->sparseResidency2Samples && !supported10Features.sparseResidency2Samples) ||
        (requiredDevice10Features->sparseResidency4Samples && !supported10Features.sparseResidency4Samples) ||
        (requiredDevice10Features->sparseResidency8Samples && !supported10Features.sparseResidency8Samples) ||
        (requiredDevice10Features->sparseResidency16Samples && !supported10Features.sparseResidency16Samples) ||
        (requiredDevice10Features->sparseResidencyAliased && !supported10Features.sparseResidencyAliased) ||
        (requiredDevice10Features->variableMultisampleRate && !supported10Features.variableMultisampleRate) ||
        (requiredDevice10Features->inheritedQueries && !supported10Features.inheritedQueries))
        {
            supportsAllFeatures = false;
        }
        // clang-format on
    }

    if (supportsAllFeatures && requiredDevice11Features) {
        // clang-format off
        if ((deviceApiVersion < vk::ApiVersion11) ||
        (requiredDevice11Features->storageBuffer16BitAccess && !supported11Features.storageBuffer16BitAccess) ||
        (requiredDevice11Features->uniformAndStorageBuffer16BitAccess && !supported11Features.uniformAndStorageBuffer16BitAccess) ||
        (requiredDevice11Features->storagePushConstant16 && !supported11Features.storagePushConstant16) ||
        (requiredDevice11Features->storageInputOutput16 && !supported11Features.storageInputOutput16) ||
        (requiredDevice11Features->multiview && !supported11Features.multiview) ||
        (requiredDevice11Features->multiviewGeometryShader && !supported11Features.multiviewGeometryShader) ||
        (requiredDevice11Features->multiviewTessellationShader && !supported11Features.multiviewTessellationShader) ||
        (requiredDevice11Features->variablePointersStorageBuffer && !supported11Features.variablePointersStorageBuffer) ||
        (requiredDevice11Features->variablePointers && !supported11Features.variablePointers) ||
        (requiredDevice11Features->protectedMemory && !supported11Features.protectedMemory) ||
        (requiredDevice11Features->samplerYcbcrConversion && !supported11Features.samplerYcbcrConversion) ||
        (requiredDevice11Features->shaderDrawParameters && !supported11Features.shaderDrawParameters))
        {
            supportsAllFeatures = false;
        }
        // clang-format on
    }

    if (supportsAllFeatures && requiredDevice12Features) {
        // clang-format off
        if ((deviceApiVersion < vk::ApiVersion12) ||
        (requiredDevice12Features->samplerMirrorClampToEdge && !supported12Features.samplerMirrorClampToEdge) ||
        (requiredDevice12Features->drawIndirectCount && !supported12Features.drawIndirectCount) ||
        (requiredDevice12Features->storageBuffer8BitAccess && !supported12Features.storageBuffer8BitAccess) ||
        (requiredDevice12Features->uniformAndStorageBuffer8BitAccess && !supported12Features.uniformAndStorageBuffer8BitAccess) ||
        (requiredDevice12Features->storagePushConstant8 && !supported12Features.storagePushConstant8) ||
        (requiredDevice12Features->shaderBufferInt64Atomics && !supported12Features.shaderBufferInt64Atomics) ||
        (requiredDevice12Features->shaderSharedInt64Atomics && !supported12Features.shaderSharedInt64Atomics) ||
        (requiredDevice12Features->shaderFloat16 && !supported12Features.shaderFloat16) ||
        (requiredDevice12Features->shaderInt8 && !supported12Features.shaderInt8) ||
        (requiredDevice12Features->descriptorIndexing && !supported12Features.descriptorIndexing) ||
        (requiredDevice12Features->shaderInputAttachmentArrayDynamicIndexing && !supported12Features.shaderInputAttachmentArrayDynamicIndexing) ||
        (requiredDevice12Features->shaderUniformTexelBufferArrayDynamicIndexing && !supported12Features.shaderUniformTexelBufferArrayDynamicIndexing) ||
        (requiredDevice12Features->shaderStorageTexelBufferArrayDynamicIndexing && !supported12Features.shaderStorageTexelBufferArrayDynamicIndexing) ||
        (requiredDevice12Features->shaderUniformBufferArrayNonUniformIndexing && !supported12Features.shaderUniformBufferArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderSampledImageArrayNonUniformIndexing && !supported12Features.shaderSampledImageArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderStorageBufferArrayNonUniformIndexing && !supported12Features.shaderStorageBufferArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderStorageImageArrayNonUniformIndexing && !supported12Features.shaderStorageImageArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderInputAttachmentArrayNonUniformIndexing && !supported12Features.shaderInputAttachmentArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderUniformTexelBufferArrayNonUniformIndexing && !supported12Features.shaderUniformTexelBufferArrayNonUniformIndexing) ||
        (requiredDevice12Features->shaderStorageTexelBufferArrayNonUniformIndexing && !supported12Features.shaderStorageTexelBufferArrayNonUniformIndexing) ||
        (requiredDevice12Features->descriptorBindingUniformBufferUpdateAfterBind && !supported12Features.descriptorBindingUniformBufferUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingSampledImageUpdateAfterBind && !supported12Features.descriptorBindingSampledImageUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingStorageImageUpdateAfterBind && !supported12Features.descriptorBindingStorageImageUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingStorageBufferUpdateAfterBind && !supported12Features.descriptorBindingStorageBufferUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingUniformTexelBufferUpdateAfterBind && !supported12Features.descriptorBindingUniformTexelBufferUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingStorageTexelBufferUpdateAfterBind && !supported12Features.descriptorBindingStorageTexelBufferUpdateAfterBind) ||
        (requiredDevice12Features->descriptorBindingUpdateUnusedWhilePending && !supported12Features.descriptorBindingUpdateUnusedWhilePending) ||
        (requiredDevice12Features->descriptorBindingPartiallyBound && !supported12Features.descriptorBindingPartiallyBound) ||
        (requiredDevice12Features->descriptorBindingVariableDescriptorCount && !supported12Features.descriptorBindingVariableDescriptorCount) ||
        (requiredDevice12Features->runtimeDescriptorArray && !supported12Features.runtimeDescriptorArray) ||
        (requiredDevice12Features->samplerFilterMinmax && !supported12Features.samplerFilterMinmax) ||
        (requiredDevice12Features->scalarBlockLayout && !supported12Features.scalarBlockLayout) ||
        (requiredDevice12Features->imagelessFramebuffer && !supported12Features.imagelessFramebuffer) ||
        (requiredDevice12Features->uniformBufferStandardLayout && !supported12Features.uniformBufferStandardLayout) ||
        (requiredDevice12Features->shaderSubgroupExtendedTypes && !supported12Features.shaderSubgroupExtendedTypes) ||
        (requiredDevice12Features->separateDepthStencilLayouts && !supported12Features.separateDepthStencilLayouts) ||
        (requiredDevice12Features->hostQueryReset && !supported12Features.hostQueryReset) ||
        (requiredDevice12Features->timelineSemaphore && !supported12Features.timelineSemaphore) ||
        (requiredDevice12Features->bufferDeviceAddress && !supported12Features.bufferDeviceAddress) ||
        (requiredDevice12Features->bufferDeviceAddressCaptureReplay && !supported12Features.bufferDeviceAddressCaptureReplay) ||
        (requiredDevice12Features->bufferDeviceAddressMultiDevice && !supported12Features.bufferDeviceAddressMultiDevice) ||
        (requiredDevice12Features->vulkanMemoryModel && !supported12Features.vulkanMemoryModel) ||
        (requiredDevice12Features->vulkanMemoryModelDeviceScope && !supported12Features.vulkanMemoryModelDeviceScope) ||
        (requiredDevice12Features->vulkanMemoryModelAvailabilityVisibilityChains && !supported12Features.vulkanMemoryModelAvailabilityVisibilityChains) ||
        (requiredDevice12Features->shaderOutputViewportIndex && !supported12Features.shaderOutputViewportIndex) ||
        (requiredDevice12Features->shaderOutputLayer && !supported12Features.shaderOutputLayer) ||
        (requiredDevice12Features->subgroupBroadcastDynamicId && !supported12Features.subgroupBroadcastDynamicId))
        {
            supportsAllFeatures = false;
        }
        // clang-format on
    }

    if (supportsAllFeatures && requiredDevice13Features) {
        // clang-format off
        if ((deviceApiVersion < vk::ApiVersion13) ||
        (requiredDevice13Features->robustImageAccess && !supported13Features.robustImageAccess) ||
        (requiredDevice13Features->inlineUniformBlock && !supported13Features.inlineUniformBlock) ||
        (requiredDevice13Features->descriptorBindingInlineUniformBlockUpdateAfterBind && !supported13Features.descriptorBindingInlineUniformBlockUpdateAfterBind) ||
        (requiredDevice13Features->pipelineCreationCacheControl && !supported13Features.pipelineCreationCacheControl) ||
        (requiredDevice13Features->privateData && !supported13Features.privateData) ||
        (requiredDevice13Features->shaderDemoteToHelperInvocation && !supported13Features.shaderDemoteToHelperInvocation) ||
        (requiredDevice13Features->shaderTerminateInvocation && !supported13Features.shaderTerminateInvocation) ||
        (requiredDevice13Features->subgroupSizeControl && !supported13Features.subgroupSizeControl) ||
        (requiredDevice13Features->computeFullSubgroups && !supported13Features.computeFullSubgroups) ||
        (requiredDevice13Features->synchronization2 && !supported13Features.synchronization2) ||
        (requiredDevice13Features->textureCompressionASTC_HDR && !supported13Features.textureCompressionASTC_HDR) ||
        (requiredDevice13Features->shaderZeroInitializeWorkgroupMemory && !supported13Features.shaderZeroInitializeWorkgroupMemory) ||
        (requiredDevice13Features->dynamicRendering && !supported13Features.dynamicRendering) ||
        (requiredDevice13Features->shaderIntegerDotProduct && !supported13Features.shaderIntegerDotProduct) ||
        (requiredDevice13Features->maintenance4 && !supported13Features.maintenance4))
        {
            supportsAllFeatures = false;
        }
        // clang-format on
    }
    return supportsAllFeatures;
}

}   // namespace renderer
