#include "Game.hpp"

#include "core/GraphicsPipelineBuilder.hpp"
#include "core/Logger.hpp"
#include "core/Utils.hpp"

// libs
#include <SDL.h>
#include <SDL_vulkan.h>

// std
#include <cstdint>
#include <limits>
#include <vector>

namespace game
{

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow("Voxel game", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr);
    std::vector<const char*> requiredInstanceExtensions(count);
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, requiredInstanceExtensions.data());

    std::vector<const char*> requiredDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

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
    DEBUG("Successfully created graphics pipeline\n");
    initFrameData();
    DEBUG("Successfully created frame data\n");
}

Game::~Game() noexcept
{
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Game::createGraphicsPipeline()
{
    core::GraphicsPipelineBuilder builder(m_vkContext.device());

    builder.setShaders("../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv");
    m_graphicsPipeline = builder.build(m_vkContext.swapchainColorFormat());
}

void Game::initFrameData()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo {.sType = vk::StructureType::eCommandPoolCreateInfo,
                                                     .pNext = nullptr,
                                                     .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                     .queueFamilyIndex = m_vkContext.graphicsQueueFamilyIndex()};

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {.sType = vk::StructureType::eCommandBufferAllocateInfo,
                                                             .pNext = nullptr,
                                                             .commandPool = nullptr,
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = 1};

    vk::SemaphoreCreateInfo semaphoreCreateInfo {.sType = vk::StructureType::eSemaphoreCreateInfo,
                                                 .pNext = nullptr,
                                                 .flags = {}};

    vk::FenceCreateInfo fenceCreateInfo {.sType = vk::StructureType::eFenceCreateInfo,
                                         .pNext = nullptr,
                                         .flags = vk::FenceCreateFlagBits::eSignaled};

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frameData[i].commandPool = m_vkContext.device().createCommandPoolUnique(commandPoolCreateInfo);
        commandBufferAllocateInfo.commandPool = *m_frameData[i].commandPool;
        m_frameData[i].commandBuffer =
            std::move(m_vkContext.device().allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

        m_frameData[i].swapchainSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        m_frameData[i].renderSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        m_frameData[i].renderFence = m_vkContext.device().createFenceUnique(fenceCreateInfo);
    }
}

void Game::drawFrame()
{
    const auto& device = m_vkContext.device();
    const auto& swapchain = m_vkContext.swapchain();
    const auto& frameData = m_frameData[m_frameCount % MAX_FRAMES_IN_FLIGHT];

    [[maybe_unused]] auto fenceRes =
        device.waitForFences(*frameData.renderFence, vk::True, std::numeric_limits<uint64_t>::max());
    device.resetFences(*frameData.renderFence);

    auto imgRes =
        device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), *frameData.swapchainSemaphore);

    if (imgRes.result == vk::Result::eErrorOutOfDateKHR) {
        m_vkContext.recreateSwapchain();
        return;
    }

    const auto& commandBuffer = *frameData.commandBuffer;

    // Begin recording and rendering
    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {.sType = vk::StructureType::eCommandBufferBeginInfo,
                                                       .pNext = nullptr,
                                                       .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                                       .pInheritanceInfo = nullptr};

    commandBuffer.begin(commandBufferBeginInfo);

    core::utils::transitionImage(commandBuffer,
                                 m_vkContext.swapchainImage(imgRes.value),
                                 vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eColorAttachmentOptimal);

    vk::RenderingAttachmentInfo colorAttachment {.sType = vk::StructureType::eRenderingAttachmentInfo,
                                                 .pNext = nullptr,
                                                 .imageView = m_vkContext.swapchainImageView(imgRes.value),
                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                 .resolveMode = {},
                                                 .resolveImageView = {},
                                                 .resolveImageLayout = {},
                                                 .loadOp = vk::AttachmentLoadOp::eLoad,
                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                 .clearValue = {}};

    vk::RenderingInfo renderingInfo {
        .sType = vk::StructureType::eRenderingInfo,
        .pNext = nullptr,
        .flags = {},
        .renderArea = {vk::Offset2D {0, 0}, m_vkContext.swapchainExtent()},
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = nullptr,
        .pStencilAttachment = nullptr
    };

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);

    // set dynamic viewport and scissor
    vk::Viewport viewport {.x = 0.f,
                           .y = 0.f,
                           .width = static_cast<float>(m_vkContext.swapchainExtent().width),
                           .height = static_cast<float>(m_vkContext.swapchainExtent().height),
                           .minDepth = 0.f,
                           .maxDepth = 1.f};
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor {
        .offset = {0, 0},
        .extent = m_vkContext.swapchainExtent()
    };
    commandBuffer.setScissor(0, scissor);

    // draw
    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRendering();

    core::utils::transitionImage(commandBuffer,
                                 m_vkContext.swapchainImage(imgRes.value),
                                 vk::ImageLayout::eColorAttachmentOptimal,
                                 vk::ImageLayout::ePresentSrcKHR);

    commandBuffer.end();

    // Submit
    vk::CommandBufferSubmitInfo commandBufferSubmitInfo {.sType = vk::StructureType::eCommandBufferSubmitInfo,
                                                         .pNext = nullptr,
                                                         .commandBuffer = commandBuffer,
                                                         .deviceMask = 0};

    vk::SemaphoreSubmitInfo waitInfo {.sType = vk::StructureType::eSemaphoreSubmitInfo,
                                      .pNext = nullptr,
                                      .semaphore = *frameData.swapchainSemaphore,
                                      .value = 1,
                                      .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                      .deviceIndex = 0};

    vk::SemaphoreSubmitInfo signalInfo {.sType = vk::StructureType::eSemaphoreSubmitInfo,
                                        .pNext = nullptr,
                                        .semaphore = *frameData.renderSemaphore,
                                        .value = 1,
                                        .stageMask = vk::PipelineStageFlagBits2::eAllGraphics,
                                        .deviceIndex = 0};

    vk::SubmitInfo2 submitInfo2 {.sType = vk::StructureType::eSubmitInfo2,
                                 .pNext = nullptr,
                                 .flags = {},
                                 .waitSemaphoreInfoCount = 1,
                                 .pWaitSemaphoreInfos = &waitInfo,
                                 .commandBufferInfoCount = 1,
                                 .pCommandBufferInfos = &commandBufferSubmitInfo,
                                 .signalSemaphoreInfoCount = 1,
                                 .pSignalSemaphoreInfos = &signalInfo};

    m_vkContext.graphicsQueue().submit2(submitInfo2, *frameData.renderFence);

    // Present
    vk::PresentInfoKHR presentInfo {.sType = vk::StructureType::ePresentInfoKHR,
                                    .pNext = nullptr,
                                    .waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &*frameData.renderSemaphore,
                                    .swapchainCount = 1,
                                    .pSwapchains = &swapchain,
                                    .pImageIndices = &imgRes.value,
                                    .pResults = nullptr};

    auto presentRes = m_vkContext.presentQueue().presentKHR(presentInfo);

    if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR) {
        m_vkContext.recreateSwapchain();
    }

    ++m_frameCount;
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
            drawFrame();
        }
    }

    m_vkContext.device().waitIdle();
}

}   // namespace game
