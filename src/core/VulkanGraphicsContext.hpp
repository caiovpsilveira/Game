#ifndef CORE_VULKAN_GRAPHICS_CONTEXT_H
#define CORE_VULKAN_GRAPHICS_CONTEXT_H

// libs
#include <vulkan/vulkan.hpp>

// std
#include <optional>
#include <span>

struct SDL_Window;
struct VmaAllocator_T;

namespace core
{

using PFN_presentModeKHRselector = vk::PresentModeKHR (*)(std::span<const vk::PresentModeKHR>);
using PFN_surfaceFormatKHRselector = vk::SurfaceFormatKHR (*)(std::span<const vk::SurfaceFormatKHR>);

/*!
 * Structure to configure the parameters required for creating a @ref VulkanGraphicsContext instance.
 */
struct VulkanGraphicsContextCreateInfo {
    /*!
     * Initializes every member to 0, false, nullptr or the default constructor.
     * Non-required parameters will be initialized with the "none" semantic.
     */
    VulkanGraphicsContextCreateInfo() noexcept;

    /*!
     * @param vulkanApiVersion the highest version of Vulkan that the application requires.
     * - This MUST be encoded with the vulkan version numbers, which can be achieved
     *   by using vk::makeApiVersion(v, M, m, p).
     * - This MUST be greater than or equal to VK_API_VERSION_1_0.
     * - If the machine Vulkan version < vulkanApiVersion, the constructor will throw, as the machine does not support
     *   the application requirements.
     * - The Khronos validation layers will treat apiVersion as the highest API version of the application targets,
     *   and will validate API usage against the minimum of that version and the implementation version.
     */
    uint32_t vulkanApiVersion;

    /*!
     * @param requiredInstanceExtensions array-view of null-terminated UTF-8 strings containing the names of
     * extensions to enable for the created instance.
     * - This MUST include "VK_KHR_surface" and the specific platform surface extension, such as
     * "VK_KHR_win32_surface" or "VK_KHR_xcb_surface".
     */
    std::span<const char* const> requiredInstanceExtensions;

    /*!
     * @param enableValidationLayersIfSupported a flag to enable validation layers, if supported by the Vulkan
     * instance.
     * - If this flag is true and the validation layers are not supported, the instance creation will proceed
     * without requesting the validation layers.
     */
    bool enableValidationLayersIfSupported;

    /*!
     * @param enableDebugMessengerIfSupported a flag to create a VkDebugUtilsMessengerEXT instance, if supported by
     * the Vulkan instance.
     * - If this flag is true and the debug utils messenger extension is not supported, the instance creation will
     *   proceed without requesting the extension.
     * - If "VK_EXT_debug_utils" is within "requiredInstanceExtensions", this flag will effectively have no effect,
     * and the instance creation will fail if it does not support the Debug Utils Messenger extension.
     */
    bool enableDebugMessengerIfSupported;

    /*!
     * @param window A valid SDL_window, which MUST have been created with the "SDL_WINDOW_VULKAN" flag.
     */
    SDL_Window* window;

    /*!
     * @param requiredDeviceExtensions A list of required device extentions.
     * - This list MUST constain "VK_KHR_swapchain".
     * - If no physical devices can simultaneously support ALL extensions specified in this list and the
     *   other criteria specified in the constructor parameters, the constructor will throw.
     */
    std::span<const char* const> requiredDeviceExtensions;

    /*!
     * @param requiredDevice10Features An optional list of of required device 1.0 features.
     * - If this is specified and no physical device can simultaneously support ALL features specified
     *   and the other criteria specified in the constructor parameters, the constructor will throw.
     */
    std::optional<vk::PhysicalDeviceFeatures> requiredDevice10Features;

    /*!
     * @param requiredDevice11Features An optional list of of required device 1.1 features.
     * - If this is specified and no physical device can simultaneously support ALL features specified
     *   and the other criteria specified in the constructor parameters, the constructor will throw.
     */
    std::optional<vk::PhysicalDeviceVulkan11Features> requiredDevice11Features;

    /*!
     * @param requiredDevice12Features An optional list of of required device 1.2 features.
     * - If this is specified and no physical device can simultaneously support ALL features specified
     *   and the other criteria specified in the constructor parameters, the constructor will throw.
     */
    std::optional<vk::PhysicalDeviceVulkan12Features> requiredDevice12Features;

    /*!
     * @param requiredDevice13Features An optional list of of required device 1.3 features.
     * - If this is specified and no physical device can simultaneously support ALL features specified
     *   and the other criteria specified in the constructor parameters, the constructor will throw.
     */
    std::optional<vk::PhysicalDeviceVulkan13Features> requiredDevice13Features;

    /*!
     * @param pfnPresentModeKHRselector nullptr, or a pointer to a function that will choose the preferred present
     * mode, passing the supported present modes as a parameter.
     * - If this is nullptr, the used present mode will be FIFO, as it's guaranteed to be supported.
     */
    PFN_presentModeKHRselector pfnPresentModeKHRselector;

    /*!
     * @param pfnSurfaceFormatKHRselector nullptr, or a pointer to a function that will choose the preferred surface
     * format, passing the supported surface formats as a parameter.
     * - If this is nullptr, the used surface format used will be the first enumerated by the physical device,
     *   as it's guaranteed that the physical device will suport at least one surface format.
     */
    PFN_surfaceFormatKHRselector pfnSurfaceFormatKHRselector;
};

/*!
 * @brief RAII Class that manages common resources in a Vulkan Graphics application.
 *
 * This class automatically loads the necessary Vulkan entrypoints using the DispatchLoaderDynamic class provided by
 * Vulkan.hpp. It ensures that all required entrypoints, including device-specific extensions, are available for Vulkan
 * function calls.
 *
 * NOTE: you MUST use the vulkan.hpp default dispatcher function ptrs to have access to those entrypoints.
 * By default, all vulkan.hpp functions uses the default dispatcher.
 *
 * Managed Resources:
 * - A VkInstance handle, including optional debug messenger and validation layer setup.
 * - A VkSurfaceKHR handle, which abstract native platform surface or window objects for use with Vulkan.
 * - A VkSwapchainKHR handle, to present to the VkSurfaceKHR.
 * - A single VkDevice handle, that supports the VK_KHR_swapchain extension, with a single graphics queue and present
 * queue.
 * - A VMA (Vulkan Memory Allocator) handle, for memory allocation management.
 *
 * For more details about a managed resource, check the documentation of the private methods
 * @ref createInstanceAndDebug, @ref searchPhysicalDevice, @ref createLogicalDevice, @ref createAllocator and
 * @ref createSwapchain.
 */
class VulkanGraphicsContext
{
public:
    /*!
     * Default constructor. Does not acquire any resource.
     */
    VulkanGraphicsContext() noexcept = default;

    /*!
     * Constructor that acquires the resources.
     *
     * @param createInfo a reference to a @ref VulkanGraphicsContextCreateInfo containing the information about how to
     * create the VulkanGraphicsContext instance. All parameters information is available on the structure
     * documentation.
     *
     * @throws a vk::SystemError containing the error information.
     * This can be because:
     * - Vulkan is not supported by the machine,
     * - the required extensions are not supported,
     * - no physical devices are suitable for the application needs,
     * - internal issues with the host/device Vulkan calls, such as being out of memory.
     *
     * If the creation fails, all acquired resources are released.
     */
    VulkanGraphicsContext(const VulkanGraphicsContextCreateInfo& createInfo);

    VulkanGraphicsContext(const VulkanGraphicsContext&) = delete;
    VulkanGraphicsContext& operator=(const VulkanGraphicsContext&) = delete;

    VulkanGraphicsContext(VulkanGraphicsContext&&) noexcept;
    VulkanGraphicsContext& operator=(VulkanGraphicsContext&&) noexcept;

    ~VulkanGraphicsContext() noexcept;

public:
    vk::Device device() const noexcept { return m_device; }

    /*!
     * Sets the present mode to be used in the next @ref recreateSwapchain call.
     * NOTE: if the present mode is not supported, @ref recreateSwapchain will fail.
     */
    void setSwapchainPresentMode(vk::PresentModeKHR presentModeKHR) noexcept
    {
        m_currentSwapchainPresentMode = presentModeKHR;
    }

    /*!
     * Sets the surface format to be used in the next @ref recreateSwapchain call.
     * NOTE: if the surface format is not supported, @ref recreateSwapchain will fail.
     */
    void setSwapchainSurfaceFormatKHR(const vk::SurfaceFormatKHR& surfaceFormatKHR) noexcept
    {
        m_currentSwapchainSurfaceFormat = surfaceFormatKHR;
    }

    /*!
     * @brief Re-Creates the swapchain.
     *
     * Re-creates the swapchain, querying the current drawable extent from the window and using the
     * set values for the swapchain present mode and surfaceFormatKHR.
     *
     * @param window the SDL_window bound to the VkSurface, to query for the drawable extent.
     *
     * @throws a vk::SystemError if the swapchain recreation failed.
     */
    void recreateSwapchain(SDL_Window* window);

private:
    struct QueueFamiliesIndices {
        uint32_t graphicsFamilyIndex;
        uint32_t presentFamilyIndex;
    };

    /*!
     * @brief Creates a Vulkan instance and optionally sets up a debug messenger.
     *
     * This function initializes a Vulkan instance with specified parameters,
     * and optionaly creates a debug utils messenger ext instance.
     *
     * This call loads the vkGetInstanceProcAddr ptr, and after creating the instance, if successfull, loads all other
     * function pointers.
     *
     * @param vulkanApiVersion 0 or the highest version of Vulkan that the application is designed to use.
     * - This MUST be encoded with the vulkan version numbers, which can be achieved
     *   by using vk::makeApiVersion(v, M, m, p).
     * - The Khronos validation layers will treat apiVersion as the highest API version of the application targets,
     *   and will validate API usage against the minimum of that version and the implementation version.
     * - If vulkanApiVersion is not 0, then it MUST be greater than or equal to VK_API_VERSION_1_0.
     * - The variant version of the instance MUST match that requested in vulkanApiVersion.
     *
     * @param requiredInstanceExtensions array-view of null-terminated UTF-8 strings containing the names of extensions
     * to enable for the created instance.
     * - This MUST include "VK_KHR_surface" and the specific platform surface extension, such as "VK_KHR_win32_surface"
     *   or "VK_KHR_xcb_surface".
     *
     * @param enableValidationLayersIfSupported Flag to enable validation layers, if supported.
     *
     * @param enableDebugMessengerIfSupported Flag to enable debug messenger, if supported.
     * - If "VK_EXT_debug_utils" is within "requiredInstanceExtensions", this flag will effectively have no effect, and
     *   the instance creation will fail if it does not support the Debug Utils Messenger extension.
     * - The Debug Callback is set with severity flags {Verbose, Info, Warning, Error}, and with Message type flags
     *   {General, Validation, Performance}.
     * - The Debug Callback calls the core::Logger implementation, to log verbose with level TRACE, info with level
     *   INFO, warning with level WARN and error with level ERROR.
     * - If this flag is true, the instance creation and destruction events will also be captured by linking the
     *   VkDebugUtilsMessengerCreateInfoEXT in the pNext.
     *
     * @throws a vk::SystemError if instance or debug creation fails.
     */
    void createInstanceAndDebug(uint32_t vulkanApiVersion,
                                std::span<const char* const> requiredInstanceExtensions,
                                bool enableValidationLayersIfSupported,
                                bool enableDebugMessengerIfSupported);

    /*!
     * @brief Searches for a physical device that suits the application needs.
     *
     * Searches for the first physical device that:
     * - has a graphics queue family,
     * - has a queue family that supports presenting to the VkSurface,
     * - supports ALL required device extensions.
     * - supports ALL required device features.
     *
     * @param requiredDeviceExtensions a list of device extensions that a device must support.
     *
     * @param requiredDevice10Features if specified, a structure containing the 1.0 features that a device must
     * support.
     *
     * @param requiredDevice11Features if specified, a structure containing the 1.1 features that a device must
     * support.
     *
     * @param requiredDevice12Features if specified, a structure containing the 1.2 features that a device must
     * support.
     *
     * @param requriedDevice13Features if specified, a structure containing the 1.3 features that a device must
     * support.
     *
     * This method creates a copy in the parameters to modify the features.pNext.
     *
     * @throws a vk::SystemError if no devices available supports ALL the specified criteria.
     */
    void searchPhysicalDevice(std::span<const char* const> requiredDeviceExtensions,
                              const std::optional<vk::PhysicalDeviceFeatures>& requiredDevice10Features,
                              const std::optional<vk::PhysicalDeviceVulkan11Features>& requiredDevice11Features,
                              const std::optional<vk::PhysicalDeviceVulkan12Features>& requiredDevice12Features,
                              const std::optional<vk::PhysicalDeviceVulkan13Features>& requiredDevice13Features);

    /*!
     * @brief Creates the VkDevice handle for the logical device.
     *
     * Requests a single queue from the graphics queue family and a single queue from the present queue family,
     * and enables the specified extensions.
     *
     * After this call, if successfull, the device-specific function entrypoints are loaded.
     *
     * @param requiredDeviceExtensions a list of device extensions to be enabled.
     *
     * @param requiredDevice10Features an optional structure containing the 1.0 features to be enabled.
     *
     * @param requiredDevice11Features an optional structure containing the 1.1 features to be enabled.
     *
     * @param requiredDevice12Features an optional structure containing the 1.2 features to be enabled.
     *
     * @param requiredDevice13Features an optional structure containing the 1.3 features to be enabled.
     *
     * NOTE: A queue family can support both the graphics queue and the present queue. In this case, only one queue will
     * be created from this family, which will be used for both purposes.
     *
     * NOTE: This method assumes that the physical devices supports the requirements, which should be queried in
     * @ref searchPhysicalDevice.
     *
     * @throws a vk::SystemError if the device creation failed.
     */
    void createLogicalDevice(std::span<const char* const> requiredDeviceExtensions,
                             std::optional<vk::PhysicalDeviceFeatures> requiredDevice10Features,
                             std::optional<vk::PhysicalDeviceVulkan11Features> requiredDevice11Features,
                             std::optional<vk::PhysicalDeviceVulkan12Features> requiredDevice12Features,
                             std::optional<vk::PhysicalDeviceVulkan13Features> requiredDevice13Features);

    /*!
     * Creates a VMA allocator instance.
     *
     * @throws a vk::SystemError if the allocator creation failed.
     */
    void createAllocator(uint32_t vulkanApiVersion);

    /*!
     * @brief Creates a VkSwapchainKHR to present to the window surface.
     *
     * If the device graphics queue family is the same as the present queue family,
     * the image sharing mode is set to eExclusive, otherwise to eConcurrent.
     *
     * @param window the SDL_window bound to the VkSurface, to query for the drawable extent.
     *
     * @param pfnPresentModeKHRselector nullptr, or a pointer to a function that will choose the preferred present mode,
     * passing the supported present modes as a parameter.
     * - If this is nullptr, the used present mode will be FIFO, as it's guaranteed to be supported.
     *
     * @param pfnSurfaceFormatKHRselector nullptr, or a pointer to a function that will choose the preferred surface
     * format, passing the supported surface formats as a parameter.
     * - If this is nullptr, the used surface format used will be the first enumerated by the physical device,
     *   as it's guaranteed that the physical device will suport at least one surface format.
     *
     * @throws a vk::SystemError if the swapchain creation failed.
     */
    void createSwapchain(SDL_Window* window,
                         PFN_presentModeKHRselector pfnPresentModeKHRselector,
                         PFN_surfaceFormatKHRselector pfnSurfaceFormatKHRselector);

    /*!
     * Frees the acquired resources.
     */
    void cleanup() noexcept;

private:
    // PhysicalDevice is owned by instance and Queues are owned by the device
    // We shouldn't need to initialize them, and a copy on move should be enough
    // instead of exchange/swap, but just to be safe and catch something off on assertions
    // they are initialized as nullptr

    vk::Instance m_instance = nullptr;
    vk::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::SurfaceKHR m_surface = nullptr;
    vk::PhysicalDevice m_physicalDevice = nullptr;
    QueueFamiliesIndices m_queueFamiliesIndices;
    vk::Device m_device = nullptr;
    vk::Queue m_graphicsQueue = nullptr;
    vk::Queue m_presentQueue = nullptr;
    VmaAllocator_T* m_allocator = nullptr;
    vk::PresentModeKHR m_currentSwapchainPresentMode;
    vk::SurfaceFormatKHR m_currentSwapchainSurfaceFormat;
    vk::SwapchainKHR m_swapchain = nullptr;
};

}   // namespace core

#endif
