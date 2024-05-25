#include "core/Logger.hpp"
#include "core/VulkanContext.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>

int main()
{
    FATAL("test FATAL");
    FATAL("test FATAL fmt {}", 3.14);

    ERROR("test ERROR");
    ERROR("test ERROR fmt {}", 3.14);

    WARN("test WARN");
    WARN("test WARN fmt {}", 3.14);

    INFO("test INFO");
    INFO("test INFO fmt {}", 3.14);

    DEBUG("test DEBUG");
    DEBUG("test DEBUG fmt {}", 3.14);

    std::string testMessage = "Hello";
    TRACE("test TRACE");
    TRACE("test TRACE fmt {} {} {} {}", 3.14, "Hi", testMessage, true);

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
