#include "Game.hpp"

#include "core/GraphicsPipelineBuilder.hpp"

// libs
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// std
#include <cstdint>
#include <vector>

namespace game
{

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow("Voxel game", 0, 0, 800, 600, SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr);
    std::vector<const char*> requiredInstanceExtensions(count);
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, requiredInstanceExtensions.data());

    std::vector<const char*> requiredDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = true;

    core::VulkanGraphicsContextCreateInfo createInfo {};
    createInfo.vulkanApiVersion = vk::makeApiVersion(0, 1, 3, 0);
    createInfo.requiredInstanceExtensions = requiredInstanceExtensions;
    createInfo.requiredDeviceExtensions = requiredDeviceExtensions;
    createInfo.enableValidationLayersIfSupported = true;
    createInfo.enableDebugMessengerIfSupported = true;
    createInfo.window = m_window;
    createInfo.requiredDevice13Features = &features13;
    m_vkContext = core::VulkanGraphicsContext(createInfo);

    createGraphicsPipeline();
}

Game::~Game() noexcept
{
    m_vkContext.device().destroyPipeline(m_graphicsPipeline);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Game::createGraphicsPipeline()
{
    core::GraphicsPipelineBuilder builder(m_vkContext.device());

    builder.setShaders("../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv");
    m_graphicsPipeline = builder.build(m_vkContext.swapchainColorFormat());
}

void Game::run()
{
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
                break;
            }
        }
    }
}

}   // namespace game
