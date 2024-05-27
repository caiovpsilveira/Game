#ifndef CORE_VULKAN_GRAPHICS_CONTEXT_H
#define CORE_VULKAN_GRAPHICS_CONTEXT_H

// std
#include <span>

// libs
#include <vulkan/vulkan.hpp>

struct SDL_Window;
struct VmaAllocator_T;

namespace core
{

/*!
 * @brief RAII Class that manages common resources in a Vulkan Graphics application.
 *
 * This class automatically loads the necessary Vulkan entrypoints using the DispatchLoaderDynamic class provided by
 * Vulkan.hpp. It ensures that all required entrypoints, including device-specific extensions, are available for Vulkan
 * function calls.
 *
 * Note that you MUST use the vulkan.hpp default dispatcher function ptrs to have access to those entrypoints.
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
 * For more details about a managed resource, check the documentation of the private methods "createInstanceAndDebug",
 * "searchPhysicalDevice", "createLogicalDevice", "createAllocator" and "createSwapchain".
 */
class VulkanGraphicsContext
{
public:
    /*!
     * @brief Default constructor. Does not acquire any resource.
     */
    VulkanGraphicsContext() noexcept = default;

    /*!
     * @brief Constructor that acquires the resources.
     *
     * @param requiredInstanceExtensions array-view of null-terminated UTF-8 strings containing the names of extensions
     * to enable for the created instance.
     * - This MUST include "VK_KHR_surface" and the specific platform surface extension, such as "VK_KHR_win32_surface"
     * or "VK_KHR_xcb_surface".
     *
     * @param enableValidationLayersIfSupported a flag to enable validation layers, if supported by the Vulkan instance.
     * - If this flag is true and the validation layers are not supported, the instance creation will proceed without
     * requesting the validation layers.
     *
     * @param enableDebugMessengerIfSupported a flag to create a VkDebugUtilsMessengerEXT instance, if supported by the
     * Vulkan instance.
     * - If this flag is true and the debug utils messenger extension is not supported, the instance creation will
     * proceed without requesting the extension.
     * - If "VK_EXT_debug_utils" is within "requiredInstanceExtensions", this flag will effectively have no effect, and
     * the instance creation will fail if it does not support the Debug Utils Extension.
     *
     * @param window A valid SDL_window, which MUST have been created with the "SDL_WINDOW_VULKAN" flag.
     *
     * @throws a vk::SystemError containing the error information.
     * This can be because:
     * - Vulkan is not supported by the machine,
     * - the required extensions are not supported,
     * - no physical devices are suitable for the application needs,
     * - internal issues with the host/device Vulkan calls, such as being out of memory.
     */
    VulkanGraphicsContext(std::span<const char* const> requiredInstanceExtensions,
                          bool enableValidationLayersIfSupported,
                          bool enableDebugMessengerIfSupported,
                          SDL_Window* window);

    VulkanGraphicsContext(const VulkanGraphicsContext&) = delete;
    VulkanGraphicsContext& operator=(const VulkanGraphicsContext&) = delete;

    VulkanGraphicsContext(VulkanGraphicsContext&&) noexcept;
    VulkanGraphicsContext& operator=(VulkanGraphicsContext&&) noexcept;

    ~VulkanGraphicsContext() noexcept;

private:
    struct QueueFamiliesIndices {
        uint32_t graphicsFamilyIndex;
        uint32_t presentFamilyIndex;
    };

    /*!
     * @brief Creates a Vulkan instance and optionally sets up a debug messenger.
     *
     * - This function initializes a Vulkan instance with specified parameters.
     * - If validation layers or debug messenger are requested but not supported, the function proceeds without them and
     * logs a message indicating their absence.
     *
     * - This call loads the vkGetInstanceProcAddr ptr, and after creating the instance, if successfull, loads all other
     * function pointers.
     *
     * @param requiredInstanceExtensions array-view of null-terminated UTF-8 strings containing the names of extensions
     * to enable for the created instance.
     * - This MUST include "VK_KHR_surface" and the specific platform surface extension, such as "VK_KHR_win32_surface"
     * or "VK_KHR_xcb_surface".
     * @param enableValidationLayersIfSupported Flag to enable validation layers, if supported.
     * @param enableDebugMessengerIfSupported Flag to enable debug messenger, if supported.
     * - If "VK_EXT_debug_utils" is within "requiredInstanceExtensions", this flag will effectively have no effect, and
     * the instance creation will fail if it does not support the Debug Utils Extension.
     * - The Debug Callback is set with severity flags {Verbose, Info, Warning, Error}, and with Message type flags
     * {General, Validation, Performance}.
     * - The Debug Callback calls the core::Logger implementation, to log verbose with level TRACE, info with level
     * INFO, warning with level WARN and error with level ERROR.
     * - If this flag is true, the instance creation and destruction events will also be captured by linking the
     * VkDebugUtilsMessengerCreateInfoEXT in the pNext.
     *
     * @throws a vk::SystemError if instance or debug creation fails.
     */
    void createInstanceAndDebug(std::span<const char* const> requiredInstanceExtensions,
                                bool enableValidationLayersIfSupported,
                                bool enableDebugMessengerIfSupported);

    /*!
     * @brief Searches for the first physical device that:
     * - has a graphics queue family,
     * - has a queue family that supports presenting to the VkSurface,
     * - supports the VK_KHR_swapchain extension.
     *
     * @throws a vk::SystemError if none devices available matches the criteria.
     */
    void searchPhysicalDevice();

    /*!
     * @brief Creates the VkDevice handle for the logical device, requesting a single queue from the graphics queue
     * family and a single queue from the present queue family, and enables the  VK_KHR_swapchain extension.
     *
     * After this call, if successfull, the device-specific function entrypoints are loaded.
     *
     * Note: A queue family can support both the graphics queue and the present queue. In this case, only one queue will
     * be created from this family, which will be used for both purposes.
     *
     * @throws a vk::SystemError if the device creation failed.
     */
    void createLogicalDevice();

    /*!
     * @brief Creates a VMA allocator instance.
     *
     * @throws a vk::SystemError if the allocator creation failed.
     */
    void createAllocator();

    /*!
     * @brief Creates a VkSwapchainKHR to present to the window surface.
     *
     * If supported, the present mode is set to Mailbox. If not, it defaults to FIFO.
     *
     * If both are supported, the image format is set to eB8G8R8A8Srgb and the color space to eSrgbNonlinear. If one is
     * not supported, it defaults to the first VkSurfaceFormatKHR enumerated by the device.
     *
     * If the device graphics queue family is the same as the present queue family, the sharing image mode is set to
     * Exclusive, otherwise to concurrent.
     *
     * @param window the SDL_window bound to the VkSurface, to query for the drawable extent.
     *
     * @throws a vk::SystemError if the swapchain creation failed.
     */
    void createSwapchain(SDL_Window* window);

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
    vk::SwapchainKHR m_swapchain = nullptr;
};

}   // namespace core

#endif
