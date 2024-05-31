#include "core/VulkanGraphicsContext.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>
#include <vulkan/vulkan.hpp>

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Test window", 0, 0, 800, 600, SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    std::vector<const char*> ext(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, ext.data());

    std::vector<const char*> requiredDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = true;

    try {
        auto vkContext = core::VulkanGraphicsContext(vk::makeApiVersion(0, 1, 3, 0),
                                                     ext,
                                                     true,
                                                     true,
                                                     window,
                                                     requiredDeviceExtensions,
                                                     {},
                                                     {},
                                                     {},
                                                     features13,
                                                     nullptr,
                                                     nullptr);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
