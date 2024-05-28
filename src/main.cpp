#include "core/Logger.hpp"
#include "core/VulkanGraphicsContext.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>
#include <vulkan/vulkan.hpp>

int main()
{
    FATAL("test FATAL\n");
    FATAL_FMT("test FATAL fmt {}\n", 3.14);

    ERROR("test ERROR\n");
    ERROR_FMT("test ERROR fmt {}\n", 3.14);

    WARN("test WARN\n");
    WARN_FMT("test WARN fmt {}\n", 3.14);

    INFO("test INFO\n");
    INFO_FMT("test INFO fmt {}\n", 3.14);

    DEBUG("test DEBUG\n");
    DEBUG_FMT("test DEBUG fmt {}\n", 3.14);

    std::string testMessage = "Hello";
    TRACE("test TRACE\n");
    TRACE_FMT("test TRACE fmt {} {} {} {}\n", 3.14, "Hi", testMessage, true);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Test window", 0, 0, 800, 600, SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    std::vector<const char*> ext(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, ext.data());

    try {
        auto vkContext = core::VulkanGraphicsContext(vk::makeApiVersion(0, 1, 3, 0), ext, true, true, window);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
