#include "core/VulkanContext.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>

int main()
{

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Test window", 0, 0, 800, 600, SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    std::vector<const char*> ext(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, ext.data());

    auto vkContext = core::VulkanContext::createVulkanContext(ext, true, true, window);

    if (!vkContext.has_value()) {
        std::cerr << "Vulkan context creation returned with error " << vkContext.error() << std::endl;
        return -1;
    }

    return 0;
}
