#include "Game.hpp"

#include "core/Logger.hpp"
#include "renderer/GraphicsPipelineBuilder.hpp"
#include "renderer/Utils.hpp"

// libs
#include <SDL.h>
#include <SDL_vulkan.h>

// std
#include <cstdint>
#include <limits>
#include <vector>

namespace game
{

// TODO: remove
static const std::vector<renderer::Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    { {0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {  {0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    { {-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

static const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

Game::Game()
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow("Voxel game", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr);
    std::vector<const char*> requiredInstanceExtensions(count);
    SDL_Vulkan_GetInstanceExtensions(m_window, &count, requiredInstanceExtensions.data());

    std::vector<const char*> requiredDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = true;
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
    createInfo.requiredDevice12Features = &features12;
    createInfo.requiredDevice13Features = &features13;
    m_vkContext = core::VulkanGraphicsContext(createInfo);

    createGraphicsPipeline();
    DEBUG("Successfully created graphics pipeline\n");
    initTransferData();
    DEBUG("Successfully created transfer data\n");
    initFrameData();
    DEBUG("Successfully created frame data\n");

    // TODO: remove
    uploadMesh();
    DEBUG("Successfully uploaded mesh\n");
}

Game::~Game() noexcept
{
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Game::createGraphicsPipeline()
{
    renderer::GraphicsPipelineBuilder builder(m_vkContext.device());

    builder.setShaders("../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv");
    m_graphicsPipeline = builder.build(m_vkContext.swapchainColorFormat());
}

void Game::initTransferData()
{
    const auto& device = m_vkContext.device();

    vk::CommandPoolCreateInfo commandPoolCreateInfo {.sType = vk::StructureType::eCommandPoolCreateInfo,
                                                     .pNext = nullptr,
                                                     .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                     .queueFamilyIndex = m_vkContext.transferQueueFamilyIndex()};

    m_transferData.commandPool = device.createCommandPoolUnique(commandPoolCreateInfo);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {.sType = vk::StructureType::eCommandBufferAllocateInfo,
                                                             .pNext = nullptr,
                                                             .commandPool = *m_transferData.commandPool,
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = 1};

    m_transferData.commandBuffer = std::move(device.allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo {.sType = vk::StructureType::eFenceCreateInfo,
                                         .pNext = nullptr,
                                         .flags = vk::FenceCreateFlagBits::eSignaled};

    m_transferData.fence = device.createFenceUnique(fenceCreateInfo);
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

void Game::uploadMesh()
{
    const auto& device = m_vkContext.device();
    const auto& allocator = m_vkContext.allocator();
    const auto& commandBuffer = *m_transferData.commandBuffer;
    const auto& fence = *m_transferData.fence;

    const vk::DeviceSize vertexBufferSize = vertices.size() * sizeof(vertices[0]);
    const vk::DeviceSize indexBufferSize = indices.size() * sizeof(indices[0]);

    m_testMesh = renderer::Mesh(device, allocator, vertexBufferSize, indexBufferSize);

    // create staging buffer, contiguous memory containing [vertexBufferData, indexBufferData]
    renderer::AllocatedBuffer stagingBuffer(allocator,
                                            vertexBufferSize + indexBufferSize,
                                            vk::BufferUsageFlagBits::eTransferSrc,
                                            VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                            VMA_MEMORY_USAGE_AUTO);

    VmaAllocationInfo stagingAllocationInfo;
    vmaGetAllocationInfo(allocator, stagingBuffer.allocation(), &stagingAllocationInfo);
    auto& stagingData = stagingAllocationInfo.pMappedData;
    // copy vertex buffer data
    std::memcpy(stagingData, vertices.data(), vertexBufferSize);
    // copy index buffer data
    std::memcpy((char*) stagingData + vertexBufferSize, indices.data(), indexBufferSize);

    // Record vkCmdCopyBuffer from staging buffer to device mesh vertex buffer and index buffer
    [[maybe_unused]] auto fenceRes = device.waitForFences(fence, true, std::numeric_limits<uint64_t>::max());
    device.resetFences(fence);

    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {.sType = vk::StructureType::eCommandBufferBeginInfo,
                                                       .pNext = nullptr,
                                                       .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                                       .pInheritanceInfo = nullptr};

    commandBuffer.begin(commandBufferBeginInfo);

    vk::BufferCopy vertexCopyRegion {.srcOffset = 0, .dstOffset = 0, .size = vertexBufferSize};
    commandBuffer.copyBuffer(stagingBuffer.buffer(), m_testMesh.vertexBuffer(), vertexCopyRegion);

    vk::BufferCopy indexCopyRegion {.srcOffset = vertexBufferSize, .dstOffset = 0, .size = indexBufferSize};
    commandBuffer.copyBuffer(stagingBuffer.buffer(), m_testMesh.indexBuffer(), indexCopyRegion);

    commandBuffer.end();

    // Submit and wait
    vk::CommandBufferSubmitInfo commandBufferSubmitInfo {.sType = vk::StructureType::eCommandBufferSubmitInfo,
                                                         .pNext = nullptr,
                                                         .commandBuffer = commandBuffer,
                                                         .deviceMask = 0};

    vk::SubmitInfo2 submitInfo2 {.sType = vk::StructureType::eSubmitInfo2,
                                 .pNext = nullptr,
                                 .flags = {},
                                 .waitSemaphoreInfoCount = 0,
                                 .pWaitSemaphoreInfos = nullptr,
                                 .commandBufferInfoCount = 1,
                                 .pCommandBufferInfos = &commandBufferSubmitInfo,
                                 .signalSemaphoreInfoCount = 0,
                                 .pSignalSemaphoreInfos = nullptr};

    m_vkContext.transferQueue().submit2(submitInfo2, fence);
    fenceRes = device.waitForFences(fence, true, std::numeric_limits<uint64_t>::max());
}

void Game::drawFrame()
{
    const auto& device = m_vkContext.device();
    const auto& swapchain = m_vkContext.swapchain();
    const auto& frameData = m_frameData[m_frameCount % MAX_FRAMES_IN_FLIGHT];
    const auto& commandBuffer = *frameData.commandBuffer;

    [[maybe_unused]] auto fenceRes =
        device.waitForFences(*frameData.renderFence, vk::True, std::numeric_limits<uint64_t>::max());
    device.resetFences(*frameData.renderFence);

    auto imgRes =
        device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), *frameData.swapchainSemaphore);

    if (imgRes.result == vk::Result::eErrorOutOfDateKHR) {
        m_vkContext.recreateSwapchain();
        return;
    }

    // Begin recording and rendering
    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {.sType = vk::StructureType::eCommandBufferBeginInfo,
                                                       .pNext = nullptr,
                                                       .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                                       .pInheritanceInfo = nullptr};

    commandBuffer.begin(commandBufferBeginInfo);

    renderer::utils::transitionImage(commandBuffer,
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
    commandBuffer.bindVertexBuffers(0, m_testMesh.vertexBuffer(), {0});
    commandBuffer.bindIndexBuffer(m_testMesh.indexBuffer(), 0, vk::IndexType::eUint32);
    commandBuffer.drawIndexed(m_testMesh.numIndices(), 1, 0, 0, 0);

    commandBuffer.endRendering();

    renderer::utils::transitionImage(commandBuffer,
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
