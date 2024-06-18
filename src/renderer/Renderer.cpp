#include "Renderer.hpp"

#include "DescriptorSetLayoutBuilder.hpp"
#include "GraphicsPipelineBuilder.hpp"
#include "Utils.hpp"
#include "core/Logger.hpp"

// libs
#include <SDL_vulkan.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

// std
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

namespace renderer
{

std::vector<Vertex> vertices {
    // left face (white)
    { {-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
    {   {-.5f, .5f, .5f}, {.9f, .9f, .9f}},
    {  {-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
    {  {-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

    // right face (yellow)
    {  {.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
    {    {.5f, .5f, .5f}, {.8f, .8f, .1f}},
    {   {.5f, -.5f, .5f}, {.8f, .8f, .1f}},
    {   {.5f, .5f, -.5f}, {.8f, .8f, .1f}},

    // top face (orange, remember y axis points down)
    { {-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
    {   {.5f, -.5f, .5f}, {.9f, .6f, .1f}},
    {  {-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
    {  {.5f, -.5f, -.5f}, {.9f, .6f, .1f}},

    // bottom face (red)
    {  {-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
    {    {.5f, .5f, .5f}, {.8f, .1f, .1f}},
    {   {-.5f, .5f, .5f}, {.8f, .1f, .1f}},
    {   {.5f, .5f, -.5f}, {.8f, .1f, .1f}},

    // nose face (blue)
    { {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
    {   {.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
    {  {-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
    {  {.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},

    // tail face (green)
    {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
    {  {.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
    { {-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
    { {.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
};

std::vector<uint32_t> indices = {0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
                                 12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21};

Renderer::Renderer(SDL_Window* window)
{
    uint32_t count;
    SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
    std::vector<const char*> requiredInstanceExtensions(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, requiredInstanceExtensions.data());

    std::vector<const char*> requiredDeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = true;
    vk::PhysicalDeviceVulkan13Features features13 {};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VulkanGraphicsContextCreateInfo createInfo {};
    createInfo.vulkanApiVersion = vk::makeApiVersion(0, 1, 3, 0);
    createInfo.requiredInstanceExtensions = requiredInstanceExtensions;
    createInfo.requiredDeviceExtensions = requiredDeviceExtensions;
    createInfo.enableValidationLayersIfSupported = true;
    createInfo.enableDebugMessengerIfSupported = true;
    createInfo.window = window;
    createInfo.requiredDevice12Features = &features12;
    createInfo.requiredDevice13Features = &features13;
    m_vkContext = VulkanGraphicsContext(createInfo);

    createGraphicsPipeline();
    initTransferCommandData();
    initFrameCommandData();
    initFrameUBOs();
    uploadMesh();
}

void Renderer::createGraphicsPipeline()
{
    DescriptorSetLayoutBuilder descriptorSetLayoutBuilder(m_vkContext.device());
    descriptorSetLayoutBuilder.addBinding(vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    vk::UniqueDescriptorSetLayout descriptorSetLayout = descriptorSetLayoutBuilder.build();

    GraphicsPipelineBuilder pipelineBuilder(m_vkContext.device());
    pipelineBuilder.setShaders("../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv");
    pipelineBuilder.addDescriptorSetLayout(std::move(descriptorSetLayout));
    m_graphicsPipeline = pipelineBuilder.build(m_vkContext.swapchainColorFormat());
    DEBUG("Successfully created graphics pipeline\n");
}

void Renderer::initTransferCommandData()
{
    const auto& device = m_vkContext.device();

    vk::CommandPoolCreateInfo commandPoolCreateInfo {.sType = vk::StructureType::eCommandPoolCreateInfo,
                                                     .pNext = nullptr,
                                                     .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                     .queueFamilyIndex = m_vkContext.transferQueueFamilyIndex()};

    m_transferCommandData.commandPool = device.createCommandPoolUnique(commandPoolCreateInfo);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {.sType = vk::StructureType::eCommandBufferAllocateInfo,
                                                             .pNext = nullptr,
                                                             .commandPool = *m_transferCommandData.commandPool,
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = 1};

    m_transferCommandData.commandBuffer = std::move(device.allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo {.sType = vk::StructureType::eFenceCreateInfo,
                                         .pNext = nullptr,
                                         .flags = vk::FenceCreateFlagBits::eSignaled};

    m_transferCommandData.fence = device.createFenceUnique(fenceCreateInfo);
    DEBUG("Successfully created transfer command data\n");
}

void Renderer::initFrameCommandData()
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

    for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frameCommandData[i].commandPool = m_vkContext.device().createCommandPoolUnique(commandPoolCreateInfo);
        commandBufferAllocateInfo.commandPool = *m_frameCommandData[i].commandPool;
        m_frameCommandData[i].commandBuffer =
            std::move(m_vkContext.device().allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

        m_frameCommandData[i].swapchainSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        m_frameCommandData[i].renderSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        m_frameCommandData[i].renderFence = m_vkContext.device().createFenceUnique(fenceCreateInfo);
    }
    DEBUG("Successfully created frame command data\n");
}

void Renderer::initFrameUBOs()
{
    for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frameUBOs[i] =
            AllocatedBuffer(m_vkContext.allocator(),
                            sizeof(UniformBufferData),
                            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                            0,
                            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    }
    DEBUG("Successfully created frame UBOs\n");
}

void Renderer::uploadMesh()
{
    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].pos.z += .5f;
    }

    const auto& device = m_vkContext.device();
    const auto& allocator = m_vkContext.allocator();
    const auto& commandBuffer = *m_transferCommandData.commandBuffer;
    const auto& fence = *m_transferCommandData.fence;

    const vk::DeviceSize vertexBufferSize = vertices.size() * sizeof(vertices[0]);
    const vk::DeviceSize indexBufferSize = indices.size() * sizeof(indices[0]);

    m_testMesh = Mesh(device, allocator, vertexBufferSize, indexBufferSize);

    // create staging buffer, contiguous memory containing [vertexBufferData, indexBufferData]
    AllocatedBuffer stagingBuffer(allocator,
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
    std::memcpy(static_cast<char*>(stagingData) + vertexBufferSize, indices.data(), indexBufferSize);

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
    DEBUG("Successfully uploaded mesh\n");
}

void Renderer::drawFrame()
{
    const auto& device = m_vkContext.device();
    const auto& swapchain = m_vkContext.swapchain();
    const auto& swapchainExtent = m_vkContext.swapchainExtent();
    const auto& frameData = m_frameCommandData[m_frameCount % MAX_FRAMES_IN_FLIGHT];
    const auto& commandBuffer = *frameData.commandBuffer;
    const auto& frameUbo = m_frameUBOs[m_frameCount % MAX_FRAMES_IN_FLIGHT];

    [[maybe_unused]] auto fenceRes =
        device.waitForFences(*frameData.renderFence, vk::True, std::numeric_limits<uint64_t>::max());
    device.resetFences(*frameData.renderFence);

    auto imgRes =
        device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), *frameData.swapchainSemaphore);

    if (imgRes.result == vk::Result::eErrorOutOfDateKHR) {
        m_vkContext.recreateSwapchain();
        return;
    }

    // Begin recording
    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {.sType = vk::StructureType::eCommandBufferBeginInfo,
                                                       .pNext = nullptr,
                                                       .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                                       .pInheritanceInfo = nullptr};

    commandBuffer.begin(commandBufferBeginInfo);

    // update UBO
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferData ubo {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                swapchainExtent.width / static_cast<float>(swapchainExtent.height),
                                0.1f,
                                10.0f);
    ubo.proj[1][1] *= -1;
    commandBuffer.updateBuffer(frameUbo.buffer(), 0, sizeof(ubo), &ubo);

    // Begin rendering
    utils::transitionImage(commandBuffer,
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
        .renderArea = {vk::Offset2D {0, 0}, swapchainExtent},
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
                           .width = static_cast<float>(swapchainExtent.width),
                           .height = static_cast<float>(swapchainExtent.height),
                           .minDepth = 0.f,
                           .maxDepth = 1.f};
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor {
        .offset = {0, 0},
        .extent = swapchainExtent
    };
    commandBuffer.setScissor(0, scissor);

    // draw
    commandBuffer.bindVertexBuffers(0, m_testMesh.vertexBuffer(), {0});
    commandBuffer.bindIndexBuffer(m_testMesh.indexBuffer(), 0, vk::IndexType::eUint32);
    commandBuffer.drawIndexed(m_testMesh.numIndices(), 1, 0, 0, 0);

    commandBuffer.endRendering();

    utils::transitionImage(commandBuffer,
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
    device.waitIdle();
}

}   // namespace renderer
