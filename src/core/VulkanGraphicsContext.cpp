#define VULKAN_HPP_NO_SMART_HANDLE
#define VULKAN_HPP_NO_CONSTRUCTORS

#define VMA_IMPLEMENTATION

#include "VulkanGraphicsContext.hpp"

#include "Logger.hpp"
#include "Utils.hpp"

// libs
#include <SDL.h>
#include <SDL_video.h>
#include <SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// std
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace core
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
    , requiredDevice10Features()
    , requiredDevice11Features()
    , requiredDevice12Features()
    , requiredDevice13Features()
    , pfnPresentModeKHRselector(nullptr)
    , pfnSurfaceFormatKHRselector(nullptr)
{}

VulkanGraphicsContext::VulkanGraphicsContext(const VulkanGraphicsContextCreateInfo& createInfo)
{
    assert(createInfo.vulkanApiVersion >= vk::ApiVersion10);
    assert(createInfo.window);
    assert(utils::containsExtension(createInfo.requiredDeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME));

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

    try {
        createInstanceAndDebug(createInfo.vulkanApiVersion,
                               createInfo.requiredInstanceExtensions,
                               createInfo.enableValidationLayersIfSupported,
                               createInfo.enableDebugMessengerIfSupported);

        SDL_bool SDLRes =
            SDL_Vulkan_CreateSurface(createInfo.window, m_instance, reinterpret_cast<VkSurfaceKHR*>(&m_surface));
        if (SDLRes != SDL_TRUE) {
            // To be consistent with vulkan.hpp, throw a vk::SystemError
            throw vk::UnknownError(SDL_GetError());
        }
        DEBUG("Successfully created surface\n");

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

        createAllocator(createInfo.vulkanApiVersion);

        createSwapchain(createInfo.window,
                        createInfo.pfnPresentModeKHRselector,
                        createInfo.pfnSurfaceFormatKHRselector);

        assert(m_instance);
        // assert(m_debugMessenger); // Allowed, can be nullptr if debug is disabled / not supported
        assert(m_surface);
        assert(m_physicalDevice);
        assert(m_device);
        assert(m_graphicsQueue);
        assert(m_presentQueue);
        assert(m_allocator);
        assert(m_swapchain);
    } catch (...) {
        cleanup();
        FATAL("VulkanGraphicsContext creation failed\n");
        throw;
    }
}

VulkanGraphicsContext::VulkanGraphicsContext(VulkanGraphicsContext&& rhs) noexcept
    : m_instance(std::exchange(rhs.m_instance, nullptr))
    , m_debugMessenger(std::exchange(rhs.m_debugMessenger, nullptr))
    , m_surface(std::exchange(rhs.m_surface, nullptr))
    , m_physicalDevice(std::exchange(rhs.m_physicalDevice, nullptr))
    , m_queueFamiliesIndices(rhs.m_queueFamiliesIndices)
    , m_device(std::exchange(rhs.m_device, nullptr))
    , m_graphicsQueue(std::exchange(rhs.m_graphicsQueue, nullptr))
    , m_presentQueue(std::exchange(rhs.m_presentQueue, nullptr))
    , m_allocator(std::exchange(rhs.m_allocator, nullptr))
    , m_currentSwapchainPresentMode(rhs.m_currentSwapchainPresentMode)
    , m_currentSwapchainSurfaceFormat(rhs.m_currentSwapchainSurfaceFormat)
    , m_swapchain(std::exchange(rhs.m_swapchain, nullptr))
{}

VulkanGraphicsContext& VulkanGraphicsContext::operator=(VulkanGraphicsContext&& rhs) noexcept
{
    if (this != &rhs) {
        std::swap(m_instance, rhs.m_instance);
        std::swap(m_debugMessenger, rhs.m_debugMessenger);
        std::swap(m_surface, rhs.m_surface);
        std::swap(m_physicalDevice, rhs.m_physicalDevice);
        m_queueFamiliesIndices = rhs.m_queueFamiliesIndices;
        std::swap(m_device, rhs.m_device);
        std::swap(m_graphicsQueue, rhs.m_graphicsQueue);
        std::swap(m_presentQueue, rhs.m_presentQueue);
        std::swap(m_allocator, rhs.m_allocator);
        m_currentSwapchainPresentMode = rhs.m_currentSwapchainPresentMode;
        m_currentSwapchainSurfaceFormat = rhs.m_currentSwapchainSurfaceFormat;
        std::swap(m_swapchain, rhs.m_swapchain);
    }
    return *this;
}

VulkanGraphicsContext::~VulkanGraphicsContext() noexcept
{
    cleanup();
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
    bool useDebugMessenger =
        enableDebugMessengerIfSupported &&
        utils::isExtensionSupported(availableInstanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

    m_instance = vk::createInstance(instanceCreateInfo);
    DEBUG("Successfully created instance\n");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

    if (useDebugMessenger) {
        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
        DEBUG("Successfully created debug messenger\n");
    }
}

void VulkanGraphicsContext::searchPhysicalDevice(
    std::span<const char* const> requiredDeviceExtensions,
    const std::optional<vk::PhysicalDeviceFeatures>& requiredDevice10Features,
    const std::optional<vk::PhysicalDeviceVulkan11Features>& requiredDevice11Features,
    const std::optional<vk::PhysicalDeviceVulkan12Features>& requiredDevice12Features,
    const std::optional<vk::PhysicalDeviceVulkan13Features>& requiredDevice13Features)
{
    // TODO: improve physical device selection.
    // Currently selecting the first one with graphicsQueueFamily + presentQueueFamily + swapchainSupport

    auto physicalDevices = m_instance.enumeratePhysicalDevices();

    for (const auto& pd : physicalDevices) {
        auto pdProperties = pd.getProperties();
        DEBUG_FMT("Encountered physical device: {}\n", static_cast<const char*>(pdProperties.deviceName));
    }

    for (const auto& pd : physicalDevices) {
        auto queueFamiliesProperties = pd.getQueueFamilyProperties();
        std::optional<uint32_t> graphicsFamilyIndex;
        std::optional<uint32_t> presentFamilyIndex;
        for (size_t i = 0; i < queueFamiliesProperties.size(); ++i) {
            const auto& qp = queueFamiliesProperties[i];
            if (qp.queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsFamilyIndex = i;
            }
            if (pd.getSurfaceSupportKHR(i, m_surface)) {
                presentFamilyIndex = i;
            }
            if (graphicsFamilyIndex && presentFamilyIndex) {
                break;
            }
        }

        // Query swapchain support
        // The Vulkan specs requires VK_KHR_surface extension to support at least VK_PRESENT_MODE_FIFO_KHR present mode
        // (ref VkPresentModeKHR(3) Manual Page), and requires at least one VkSurfaceFormatKHR to be supported, with
        // format != undefined (ref vkGetPhysicalDeviceSurfaceFormatsKHR(3) Manual Page).
        // Just checking if the VK_KHR_swapchain is supported is enough.
        auto availablePhysicalDeviceExtensions = pd.enumerateDeviceExtensionProperties();

        bool supportsAllExtensions = true;
        for (auto ext : requiredDeviceExtensions) {
            if (!utils::isExtensionSupported(availablePhysicalDeviceExtensions, ext)) {
                supportsAllExtensions = false;
                break;
            }
        }

        vk::PhysicalDeviceFeatures2 chain;
        auto& supported10Features = chain.features;
        vk::PhysicalDeviceVulkan11Features supported11Features;
        vk::PhysicalDeviceVulkan12Features supported12Features;
        vk::PhysicalDeviceVulkan13Features supported13Features;
        chain.pNext = &supported11Features;
        supported11Features.pNext = &supported12Features;
        supported12Features.pNext = &supported13Features;

        pd.getFeatures2(&chain);

        bool supportsAllFeatures = true;
        if (requiredDevice10Features) {
            // clang-format off
            if (requiredDevice10Features->robustBufferAccess && !supported10Features.robustBufferAccess) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->fullDrawIndexUint32 && !supported10Features.fullDrawIndexUint32) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->imageCubeArray && !supported10Features.imageCubeArray) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->independentBlend && !supported10Features.independentBlend) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->geometryShader && !supported10Features.geometryShader) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->tessellationShader && !supported10Features.tessellationShader) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sampleRateShading && !supported10Features.sampleRateShading) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->dualSrcBlend && !supported10Features.dualSrcBlend) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->logicOp && !supported10Features.logicOp) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->multiDrawIndirect && !supported10Features.multiDrawIndirect) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->drawIndirectFirstInstance && !supported10Features.drawIndirectFirstInstance) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->depthClamp && !supported10Features.depthClamp) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->depthBiasClamp && !supported10Features.depthBiasClamp) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->fillModeNonSolid && !supported10Features.fillModeNonSolid) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->depthBounds && !supported10Features.depthBounds) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->wideLines && !supported10Features.wideLines) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->largePoints && !supported10Features.largePoints) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->alphaToOne && !supported10Features.alphaToOne) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->multiViewport && !supported10Features.multiViewport) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->samplerAnisotropy && !supported10Features.samplerAnisotropy) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->textureCompressionETC2 && !supported10Features.textureCompressionETC2) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->textureCompressionASTC_LDR && !supported10Features.textureCompressionASTC_LDR) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->textureCompressionBC && !supported10Features.textureCompressionBC) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->occlusionQueryPrecise && !supported10Features.occlusionQueryPrecise) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->pipelineStatisticsQuery && !supported10Features.pipelineStatisticsQuery) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->vertexPipelineStoresAndAtomics && !supported10Features.vertexPipelineStoresAndAtomics) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->fragmentStoresAndAtomics && !supported10Features.fragmentStoresAndAtomics) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderTessellationAndGeometryPointSize && !supported10Features.shaderTessellationAndGeometryPointSize) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderImageGatherExtended && !supported10Features.shaderImageGatherExtended) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageImageExtendedFormats && !supported10Features.shaderStorageImageExtendedFormats) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageImageMultisample && !supported10Features.shaderStorageImageMultisample) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageImageReadWithoutFormat && !supported10Features.shaderStorageImageReadWithoutFormat) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageImageWriteWithoutFormat && !supported10Features.shaderStorageImageWriteWithoutFormat) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderUniformBufferArrayDynamicIndexing && !supported10Features.shaderUniformBufferArrayDynamicIndexing) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderSampledImageArrayDynamicIndexing && !supported10Features.shaderSampledImageArrayDynamicIndexing) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageBufferArrayDynamicIndexing && !supported10Features.shaderStorageBufferArrayDynamicIndexing) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderStorageImageArrayDynamicIndexing && !supported10Features.shaderStorageImageArrayDynamicIndexing) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderClipDistance && !supported10Features.shaderClipDistance) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderCullDistance && !supported10Features.shaderCullDistance) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderFloat64 && !supported10Features.shaderFloat64) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderInt64 && !supported10Features.shaderInt64) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderInt16 && !supported10Features.shaderInt16) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderResourceResidency && !supported10Features.shaderResourceResidency) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->shaderResourceMinLod && !supported10Features.shaderResourceMinLod) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseBinding && !supported10Features.sparseBinding) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidencyBuffer && !supported10Features.sparseResidencyBuffer) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidencyImage2D && !supported10Features.sparseResidencyImage2D) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidencyImage3D && !supported10Features.sparseResidencyImage3D) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidency2Samples && !supported10Features.sparseResidency2Samples) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidency4Samples && !supported10Features.sparseResidency4Samples) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidency8Samples && !supported10Features.sparseResidency8Samples) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidency16Samples && !supported10Features.sparseResidency16Samples) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->sparseResidencyAliased && !supported10Features.sparseResidencyAliased) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->variableMultisampleRate && !supported10Features.variableMultisampleRate) { supportsAllFeatures = false; break; }
            if (requiredDevice10Features->inheritedQueries && !supported10Features.inheritedQueries) { supportsAllFeatures = false; break; }
            // clang-format on
        }

        if (supportsAllFeatures && requiredDevice11Features) {
            // clang-format off
            if (requiredDevice11Features->storageBuffer16BitAccess && !supported11Features.storageBuffer16BitAccess) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->uniformAndStorageBuffer16BitAccess && !supported11Features.uniformAndStorageBuffer16BitAccess) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->storagePushConstant16 && !supported11Features.storagePushConstant16) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->storageInputOutput16 && !supported11Features.storageInputOutput16) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->multiview && !supported11Features.multiview) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->multiviewGeometryShader && !supported11Features.multiviewGeometryShader) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->multiviewTessellationShader && !supported11Features.multiviewTessellationShader) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->variablePointersStorageBuffer && !supported11Features.variablePointersStorageBuffer) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->variablePointers && !supported11Features.variablePointers) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->protectedMemory && !supported11Features.protectedMemory) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->samplerYcbcrConversion && !supported11Features.samplerYcbcrConversion) { supportsAllFeatures = false; break; }
            if (requiredDevice11Features->shaderDrawParameters && !supported11Features.shaderDrawParameters) { supportsAllFeatures = false; break; }
            // clang-format on
        }

        if (supportsAllFeatures && requiredDevice12Features) {
            // clang-format off
            if (requiredDevice12Features->samplerMirrorClampToEdge && !supported12Features.samplerMirrorClampToEdge) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->drawIndirectCount && !supported12Features.drawIndirectCount) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->storageBuffer8BitAccess && !supported12Features.storageBuffer8BitAccess) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->uniformAndStorageBuffer8BitAccess && !supported12Features.uniformAndStorageBuffer8BitAccess) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->storagePushConstant8 && !supported12Features.storagePushConstant8) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderBufferInt64Atomics && !supported12Features.shaderBufferInt64Atomics) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderSharedInt64Atomics && !supported12Features.shaderSharedInt64Atomics) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderFloat16 && !supported12Features.shaderFloat16) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderInt8 && !supported12Features.shaderInt8) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorIndexing && !supported12Features.descriptorIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderInputAttachmentArrayDynamicIndexing && !supported12Features.shaderInputAttachmentArrayDynamicIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderUniformTexelBufferArrayDynamicIndexing && !supported12Features.shaderUniformTexelBufferArrayDynamicIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderStorageTexelBufferArrayDynamicIndexing && !supported12Features.shaderStorageTexelBufferArrayDynamicIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderUniformBufferArrayNonUniformIndexing && !supported12Features.shaderUniformBufferArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderSampledImageArrayNonUniformIndexing && !supported12Features.shaderSampledImageArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderStorageBufferArrayNonUniformIndexing && !supported12Features.shaderStorageBufferArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderStorageImageArrayNonUniformIndexing && !supported12Features.shaderStorageImageArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderInputAttachmentArrayNonUniformIndexing && !supported12Features.shaderInputAttachmentArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderUniformTexelBufferArrayNonUniformIndexing && !supported12Features.shaderUniformTexelBufferArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderStorageTexelBufferArrayNonUniformIndexing && !supported12Features.shaderStorageTexelBufferArrayNonUniformIndexing) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingUniformBufferUpdateAfterBind && !supported12Features.descriptorBindingUniformBufferUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingSampledImageUpdateAfterBind && !supported12Features.descriptorBindingSampledImageUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingStorageImageUpdateAfterBind && !supported12Features.descriptorBindingStorageImageUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingStorageBufferUpdateAfterBind && !supported12Features.descriptorBindingStorageBufferUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingUniformTexelBufferUpdateAfterBind && !supported12Features.descriptorBindingUniformTexelBufferUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingStorageTexelBufferUpdateAfterBind && !supported12Features.descriptorBindingStorageTexelBufferUpdateAfterBind) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingUpdateUnusedWhilePending && !supported12Features.descriptorBindingUpdateUnusedWhilePending) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingPartiallyBound && !supported12Features.descriptorBindingPartiallyBound) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->descriptorBindingVariableDescriptorCount && !supported12Features.descriptorBindingVariableDescriptorCount) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->runtimeDescriptorArray && !supported12Features.runtimeDescriptorArray) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->samplerFilterMinmax && !supported12Features.samplerFilterMinmax) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->scalarBlockLayout && !supported12Features.scalarBlockLayout) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->imagelessFramebuffer && !supported12Features.imagelessFramebuffer) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->uniformBufferStandardLayout && !supported12Features.uniformBufferStandardLayout) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderSubgroupExtendedTypes && !supported12Features.shaderSubgroupExtendedTypes) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->separateDepthStencilLayouts && !supported12Features.separateDepthStencilLayouts) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->hostQueryReset && !supported12Features.hostQueryReset) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->timelineSemaphore && !supported12Features.timelineSemaphore) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->bufferDeviceAddress && !supported12Features.bufferDeviceAddress) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->bufferDeviceAddressCaptureReplay && !supported12Features.bufferDeviceAddressCaptureReplay) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->bufferDeviceAddressMultiDevice && !supported12Features.bufferDeviceAddressMultiDevice) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->vulkanMemoryModel && !supported12Features.vulkanMemoryModel) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->vulkanMemoryModelDeviceScope && !supported12Features.vulkanMemoryModelDeviceScope) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->vulkanMemoryModelAvailabilityVisibilityChains && !supported12Features.vulkanMemoryModelAvailabilityVisibilityChains) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderOutputViewportIndex && !supported12Features.shaderOutputViewportIndex) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->shaderOutputLayer && !supported12Features.shaderOutputLayer) { supportsAllFeatures = false; break; };
            if (requiredDevice12Features->subgroupBroadcastDynamicId && !supported12Features.subgroupBroadcastDynamicId) { supportsAllFeatures = false; break; };
            // clang-format on
        }

        if (supportsAllFeatures && requiredDevice13Features) {
            // clang-format off
            if (requiredDevice13Features->robustImageAccess && !supported13Features.robustImageAccess) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->inlineUniformBlock && !supported13Features.inlineUniformBlock) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->descriptorBindingInlineUniformBlockUpdateAfterBind && !supported13Features.descriptorBindingInlineUniformBlockUpdateAfterBind) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->pipelineCreationCacheControl && !supported13Features.pipelineCreationCacheControl) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->privateData && !supported13Features.privateData) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->shaderDemoteToHelperInvocation && !supported13Features.shaderDemoteToHelperInvocation) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->shaderTerminateInvocation && !supported13Features.shaderTerminateInvocation) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->subgroupSizeControl && !supported13Features.subgroupSizeControl) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->computeFullSubgroups && !supported13Features.computeFullSubgroups) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->synchronization2 && !supported13Features.synchronization2) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->textureCompressionASTC_HDR && !supported13Features.textureCompressionASTC_HDR) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->shaderZeroInitializeWorkgroupMemory && !supported13Features.shaderZeroInitializeWorkgroupMemory) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->dynamicRendering && !supported13Features.dynamicRendering) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->shaderIntegerDotProduct && !supported13Features.shaderIntegerDotProduct) { supportsAllFeatures = false; break; }
            if (requiredDevice13Features->maintenance4 && !supported13Features.maintenance4) { supportsAllFeatures = false; break; }
            // clang-format on
        }

        if (graphicsFamilyIndex && presentFamilyIndex && supportsAllExtensions && supportsAllFeatures) {
            m_physicalDevice = pd;
            m_queueFamiliesIndices.graphicsFamilyIndex = *graphicsFamilyIndex;
            m_queueFamiliesIndices.presentFamilyIndex = *presentFamilyIndex;

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

            return;
        }
    }
    // To be consistent with vulkan.hpp, throw a vk::SystemError
    throw vk::UnknownError("No physical device matched the application requirements");
}

void VulkanGraphicsContext::createLogicalDevice(
    std::span<const char* const> requiredDeviceExtensions,
    std::optional<vk::PhysicalDeviceFeatures> requiredDevice10Features,
    std::optional<vk::PhysicalDeviceVulkan11Features> requiredDevice11Features,
    std::optional<vk::PhysicalDeviceVulkan12Features> requiredDevice12Features,
    std::optional<vk::PhysicalDeviceVulkan13Features> requiredDevice13Features)
{
    std::vector<uint32_t> uniqueQueueFamiliesIndices {m_queueFamiliesIndices.graphicsFamilyIndex,
                                                      m_queueFamiliesIndices.presentFamilyIndex};
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

    // Create a chain (linked list) of structure types for device features
    // 1.0 features is directly linked as pFeatures
    void* deviceCreateInfoPNext = nullptr;   // none

    if (requiredDevice11Features) {
        // Emplace 1.1 in the front of the chain
        deviceCreateInfoPNext = &(requiredDevice11Features.value());
    }

    if (requiredDevice12Features) {
        if (requiredDevice11Features && deviceCreateInfoPNext == &(requiredDevice11Features.value())) {
            // 1.1 is the beggining of the chain.
            requiredDevice12Features->pNext = &(requiredDevice11Features.value());
        }
        // Emplace 1.2 in the front of the chain
        deviceCreateInfoPNext = &(requiredDevice12Features.value());
    }

    if (requiredDevice13Features) {
        if (requiredDevice11Features && deviceCreateInfoPNext == &(requiredDevice11Features.value())) {
            // 1.1 is the beggining of the chain.
            requiredDevice13Features->pNext = &(requiredDevice11Features.value());
        } else if (requiredDevice12Features && deviceCreateInfoPNext == &(requiredDevice12Features.value())) {
            // 1.2 is the beggining of the chain.
            requiredDevice13Features->pNext = &(requiredDevice12Features.value());
        }
        // Emplace 1.3 in the front of the chain
        deviceCreateInfoPNext = &(requiredDevice13Features.value());
    }

    vk::DeviceCreateInfo deviceCreateInfo {
        .sType = vk::StructureType::eDeviceCreateInfo,
        .pNext = deviceCreateInfoPNext,
        .flags = {},
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = requiredDevice10Features ? &(requiredDevice10Features.value()) : nullptr};

    m_device = m_physicalDevice.createDevice(deviceCreateInfo);
    DEBUG("Successfully created logical device\n");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

    m_graphicsQueue = m_device.getQueue(m_queueFamiliesIndices.graphicsFamilyIndex, 0);
    m_presentQueue = m_device.getQueue(m_queueFamiliesIndices.presentFamilyIndex, 0);
}

void VulkanGraphicsContext::createAllocator(uint32_t vulkanApiVersion)
{
    const auto& d = VULKAN_HPP_DEFAULT_DISPATCHER;
    // Let VMA handle the function pointers loading
    VmaVulkanFunctions vulkanFunctions {};
    vulkanFunctions.vkGetInstanceProcAddr = d.vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = d.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo {.flags = 0,
                                                .physicalDevice = m_physicalDevice,
                                                .device = m_device,
                                                .preferredLargeHeapBlockSize = 0,
                                                .pAllocationCallbacks = nullptr,
                                                .pDeviceMemoryCallbacks = nullptr,
                                                .pHeapSizeLimit = nullptr,
                                                .pVulkanFunctions = &vulkanFunctions,
                                                .instance = m_instance,
                                                .vulkanApiVersion = vulkanApiVersion,
                                                .pTypeExternalMemoryHandleTypes = nullptr};

    vk::Result result = static_cast<vk::Result>(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));
    vk::detail::resultCheck(result, "vmaCreateAllocator");
    DEBUG("Successfully created vmaAllocator\n");
}

void VulkanGraphicsContext::createSwapchain(SDL_Window* window,
                                            PFN_presentModeKHRselector pfnPresentModeKHRselector,
                                            PFN_surfaceFormatKHRselector pfnSurfaceFormatKHRselector)
{
    // Select the default values for the present mode and SurfaceFormatKHR
    if (pfnPresentModeKHRselector != nullptr) {
        auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface);
        m_currentSwapchainPresentMode = pfnPresentModeKHRselector(presentModes);
    } else {
        m_currentSwapchainPresentMode = vk::PresentModeKHR::eFifo;
    }

    auto surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
    m_currentSwapchainSurfaceFormat =
        pfnSurfaceFormatKHRselector ? pfnSurfaceFormatKHRselector(surfaceFormats) : surfaceFormats[0];

    recreateSwapchain(window);
}

void VulkanGraphicsContext::recreateSwapchain(SDL_Window* window)
{
    auto surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);

    vk::Extent2D swapchainExtent;
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        swapchainExtent = surfaceCapabilities.currentExtent;
    } else {
        int w, h;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        vk::Extent2D actualExtent(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        actualExtent.width = std::clamp(actualExtent.width,
                                        surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width);

        actualExtent.height = std::clamp(actualExtent.height,
                                         surfaceCapabilities.minImageExtent.height,
                                         surfaceCapabilities.maxImageExtent.height);
        swapchainExtent = actualExtent;
    }

    uint32_t imageCount = surfaceCapabilities.maxImageCount == 0   // unlimited
                              ? surfaceCapabilities.minImageCount + 1
                              : std::min(surfaceCapabilities.maxImageCount, surfaceCapabilities.minImageCount + 1);

    vk::SharingMode imageSharingMode;
    uint32_t queueFamilyIndexCount;
    uint32_t* pQueueFamilyIndices;
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
    vk::SwapchainKHR oldSwapchain = m_swapchain;
    vk::SwapchainCreateInfoKHR swapchainCreateInfo {.sType = vk::StructureType::eSwapchainCreateInfoKHR,
                                                    .pNext = nullptr,
                                                    .flags = {},
                                                    .surface = m_surface,
                                                    .minImageCount = imageCount,
                                                    .imageFormat = m_currentSwapchainSurfaceFormat.format,
                                                    .imageColorSpace = m_currentSwapchainSurfaceFormat.colorSpace,
                                                    .imageExtent = swapchainExtent,
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

    m_swapchain = m_device.createSwapchainKHR(swapchainCreateInfo);
    DEBUG_FMT("Successfully {} swapchain\n", oldSwapchain == nullptr ? "created" : "re-created");
    if (oldSwapchain != nullptr) {
        // We could keep the retired old swapchain to try to present already retrieved images, but is this worth it?
        // in which scenarios would need a Swapchain recreation AND have the oldSwapchain NOT entered a state that
        // causes VK_ERROR_OUT_OF_DATE_KHR to be returned?
        // furthermore, does applications frequently acquire more than 1 image before presenting it?
        m_device.destroySwapchainKHR(oldSwapchain);
    }
}

void VulkanGraphicsContext::cleanup() noexcept
{
    // This is reused both on the destructor and to cleanup resouce aquired during the constructor, if something failed.
    // For the destructor, all members must be in a valid state, otherwise the constructor must have thrown

    if (m_device) {
        m_device.waitIdle();
    }
    if (m_swapchain) {
        assert(m_device);
        m_device.destroySwapchainKHR(m_swapchain);
    }
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
    }
    // Queues are owned by device
    if (m_device) {
        m_device.destroy();
    }
    // PhysicalDevice is owned by the instance
    if (m_surface) {
        assert(m_instance);
        m_instance.destroySurfaceKHR(m_surface);
    }
    if (m_debugMessenger) {
        assert(m_instance);
        m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger);
    }
    if (m_instance) {
        m_instance.destroy();
    }
}

}   // namespace core
