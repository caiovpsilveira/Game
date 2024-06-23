#include "Renderer.hpp"

#include "DescriptorSetLayoutBuilder.hpp"
#include "GraphicsPipelineBuilder.hpp"
#include "PipelineLayoutBuilder.hpp"
#include "core/Logger.hpp"

// libs
#include <SDL_vulkan.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <stb_image.h>

// std
#include <cstdint>
#include <limits>
#include <vector>

namespace renderer
{

// TODO: remove
static const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    { {0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {  {0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    { {-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

static const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

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

    initFrameCommandData();
    createUboDescriptorPool();
    allocateFrameUboBuffers();
    vk::UniqueDescriptorSetLayout uboSetLayout = createUbosDescriptorSets();
    createGraphicsPipeline(std::move(uboSetLayout));
    initTransferCommandData();

    uploadMesh();
    uploadTexture();
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

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& commandData = m_frameData[i].commandData;
        commandData.commandPool = m_vkContext.device().createCommandPoolUnique(commandPoolCreateInfo);
        commandBufferAllocateInfo.commandPool = *commandData.commandPool;
        commandData.commandBuffer =
            std::move(m_vkContext.device().allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

        commandData.swapchainSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        commandData.renderSemaphore = m_vkContext.device().createSemaphoreUnique(semaphoreCreateInfo);
        commandData.renderFence = m_vkContext.device().createFenceUnique(fenceCreateInfo);
    }
    DEBUG("Successfully created frame command data\n");
}

void Renderer::createUboDescriptorPool()
{
    vk::DescriptorPoolSize poolSize {.type = vk::DescriptorType::eUniformBuffer,
                                     .descriptorCount = MAX_FRAMES_IN_FLIGHT};

    vk::DescriptorPoolCreateInfo poolCreateInfo {.sType = vk::StructureType::eDescriptorPoolCreateInfo,
                                                 .pNext = nullptr,
                                                 .flags = {},
                                                 .maxSets = MAX_FRAMES_IN_FLIGHT,
                                                 .poolSizeCount = 1,
                                                 .pPoolSizes = &poolSize};

    m_uboDescriptorPool = m_vkContext.device().createDescriptorPoolUnique(poolCreateInfo);
    DEBUG("Successfully created UBO descriptor pool\n");
}

void Renderer::allocateFrameUboBuffers()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frameData[i].ubo =
            AllocatedBuffer(m_vkContext.allocator(),
                            sizeof(UniformBufferObject),
                            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                            0,
                            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    }
    DEBUG("Successfully allocated frame UBOs buffers\n");
}

vk::UniqueDescriptorSetLayout Renderer::createUbosDescriptorSets()
{
    const auto& device = m_vkContext.device();
    DescriptorSetLayoutBuilder descriptorSetLayoutBuilder(device);
    descriptorSetLayoutBuilder.addBinding(vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    vk::UniqueDescriptorSetLayout uboSetLayout = descriptorSetLayoutBuilder.build();

    vk::DescriptorSetLayout descriptorSetLayouts[MAX_FRAMES_IN_FLIGHT];
    std::ranges::fill(descriptorSetLayouts, *uboSetLayout);

    vk::DescriptorSetAllocateInfo allocateInfo {.sType = vk::StructureType::eDescriptorSetAllocateInfo,
                                                .pNext = nullptr,
                                                .descriptorPool = *m_uboDescriptorPool,
                                                .descriptorSetCount = std::size(descriptorSetLayouts),
                                                .pSetLayouts = descriptorSetLayouts};

    // Allocate descriptor sets
    {
        auto descriptorSetsVec = device.allocateDescriptorSets(allocateInfo);
        assert(descriptorSetsVec.size() == MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            // move to stack instead of keeping dynamic allocation
            m_frameData[i].uboDescriptorSet = descriptorSetsVec[i];
        }
    }

    // Update descriptor sets
    vk::DescriptorBufferInfo bufferInfo {.buffer = nullptr, .offset = 0, .range = sizeof(UniformBufferObject)};
    vk::WriteDescriptorSet descriptorWrite {.sType = vk::StructureType::eWriteDescriptorSet,
                                            .pNext = nullptr,
                                            .dstSet = nullptr,
                                            .dstBinding = 0,
                                            .dstArrayElement = 0,
                                            .descriptorCount = 1,
                                            .descriptorType = vk::DescriptorType::eUniformBuffer,
                                            .pImageInfo = nullptr,
                                            .pBufferInfo = &bufferInfo,
                                            .pTexelBufferView = nullptr};
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frameData = m_frameData[i];
        bufferInfo.buffer = frameData.ubo.buffer();
        descriptorWrite.dstSet = frameData.uboDescriptorSet;
        device.updateDescriptorSets(descriptorWrite, nullptr);
    }
    DEBUG("Successfully created UBO descriptor sets\n");
    return uboSetLayout;
}

void Renderer::createGraphicsPipeline(vk::UniqueDescriptorSetLayout&& uboSetLayout)
{
    const auto& device = m_vkContext.device();
    PipelineLayoutBuilder pipelineLayoutBuilder(device);
    pipelineLayoutBuilder.addDescriptorSetLayout(std::move(uboSetLayout));
    m_graphicsPipelineLayout = pipelineLayoutBuilder.build();

    GraphicsPipelineBuilder pipelineBuilder(m_vkContext);
    pipelineBuilder.setShaders("../shaders/simple_shader.vert.spv", "../shaders/simple_shader.frag.spv");
    pipelineBuilder.setPipelineLayout(*m_graphicsPipelineLayout);
    m_graphicsPipeline = pipelineBuilder.build();
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
    DEBUG("Successfully created transfer command data\n");
}

vk::UniqueCommandBuffer Renderer::beginSingleTimeTransferCommand()
{
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {.sType = vk::StructureType::eCommandBufferAllocateInfo,
                                                             .pNext = nullptr,
                                                             .commandPool = *m_transferCommandData.commandPool,
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = 1};

    vk::UniqueCommandBuffer commandBuffer =
        std::move(m_vkContext.device().allocateCommandBuffersUnique(commandBufferAllocateInfo)[0]);

    vk::CommandBufferBeginInfo commandBufferBeginInfo {.sType = vk::StructureType::eCommandBufferBeginInfo,
                                                       .pNext = nullptr,
                                                       .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                                       .pInheritanceInfo = nullptr};

    commandBuffer->begin(commandBufferBeginInfo);
    return commandBuffer;
}

void Renderer::endSingleTimeTransferCommand(vk::UniqueCommandBuffer&& commandBuffer)
{
    commandBuffer->end();

    // Submit and wait
    vk::CommandBufferSubmitInfo commandBufferSubmitInfo {.sType = vk::StructureType::eCommandBufferSubmitInfo,
                                                         .pNext = nullptr,
                                                         .commandBuffer = *commandBuffer,
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

    const auto& queue = m_vkContext.transferQueue();
    queue.submit2(submitInfo2, nullptr);
    // TODO: have a vector of fences in transferCommandData, and have a single wait instead of waiting for each
    // individual command?
    queue.waitIdle();
    m_vkContext.device().resetCommandPool(*m_transferCommandData.commandPool);
}

void Renderer::transitionImageLayout(vk::CommandBuffer commandBuffer,
                                     vk::Image image,
                                     vk::Format format,
                                     vk::ImageLayout oldLayout,
                                     vk::ImageLayout newLayout)
{
    (void) format;

    vk::PipelineStageFlags2 sourceStage;
    vk::AccessFlags2 sourceAccess;
    vk::PipelineStageFlags2 destStage;
    vk::AccessFlags2 destAccess;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        sourceAccess = {};
        destAccess = vk::AccessFlagBits2::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits2::eTopOfPipe;
        destStage = vk::PipelineStageFlagBits2::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
               newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        sourceAccess = vk::AccessFlagBits2::eTransferWrite;
        destAccess = vk::AccessFlagBits2::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits2::eTransfer;
        destStage = vk::PipelineStageFlagBits2::eFragmentShader;
    } else if ((oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) ||
               (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
                newLayout == vk::ImageLayout::ePresentSrcKHR)) {
        // TODO: review
        sourceAccess = vk::AccessFlagBits2::eMemoryWrite;
        destAccess = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
        sourceStage = vk::PipelineStageFlagBits2::eAllCommands;
        destStage = vk::PipelineStageFlagBits2::eAllCommands;
    } else {
        assert(0);
    }

    vk::ImageMemoryBarrier2 imageBarrier {
        .sType = vk::StructureType::eImageMemoryBarrier2,
        .pNext = nullptr,
        .srcStageMask = sourceStage,
        .srcAccessMask = sourceAccess,
        .dstStageMask = destStage,
        .dstAccessMask = destAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}
    };

    vk::DependencyInfo dependencyInfo {.sType = vk::StructureType::eDependencyInfo,
                                       .pNext = nullptr,
                                       .dependencyFlags = {},
                                       .memoryBarrierCount = 0,
                                       .pMemoryBarriers = nullptr,
                                       .bufferMemoryBarrierCount = 0,
                                       .pBufferMemoryBarriers = nullptr,
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &imageBarrier};

    commandBuffer.pipelineBarrier2(dependencyInfo);
}

Allocated2DImage Renderer::createTextureImage(const std::filesystem::path& path)
{
    int texWidth, texHeight, texChannels;
    using unique_stbi_uc_t = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    unique_stbi_uc_t pixels(stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha),
                            &stbi_image_free);

    if (!pixels) {
        throw std::ios_base::failure(std::string("Could not open file " + path.string()));
    }

    const auto& allocator = m_vkContext.allocator();
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    AllocatedBuffer stagingBuffer(allocator,
                                  imageSize,
                                  vk::BufferUsageFlagBits::eTransferSrc,
                                  VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                  VMA_MEMORY_USAGE_AUTO);

    VmaAllocationInfo stagingAllocationInfo = stagingBuffer.allocationInfo();
    auto& data = stagingAllocationInfo.pMappedData;
    std::memcpy(data, pixels.get(), static_cast<size_t>(imageSize));

    Allocated2DImage image(m_vkContext.device(),
                           allocator,
                           vk::Format::eR8G8B8A8Srgb,
                           {.width = static_cast<uint32_t>(texWidth), .height = static_cast<uint32_t>(texHeight)},
                           vk::ImageTiling::eOptimal,
                           vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                           0,
                           VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    auto commandBuffer = beginSingleTimeTransferCommand();

    transitionImageLayout(*commandBuffer,
                          image.image(),
                          vk::Format::eUndefined,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy region {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .mipLevel = 0,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
        .imageOffset = {.x = 0, .y = 0, .z = 0},
        .imageExtent = {.width = static_cast<uint32_t>(texWidth),
                             .height = static_cast<uint32_t>(texHeight),
                             .depth = 1}
    };
    commandBuffer->copyBufferToImage(stagingBuffer.buffer(),
                                     image.image(),
                                     vk::ImageLayout::eTransferDstOptimal,
                                     region);

    transitionImageLayout(*commandBuffer,
                          image.image(),
                          vk::Format::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);

    endSingleTimeTransferCommand(std::move(commandBuffer));

    return image;
}

void Renderer::updateUbo(vk::CommandBuffer command, vk::Buffer ubo, const vk::Extent2D& swapchainExtent)
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject uboData {};
    uboData.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    uboData.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    uboData.proj =
        glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f);
    uboData.proj[1][1] *= -1;

    command.updateBuffer(ubo, 0, sizeof(uboData), &uboData);
}

void Renderer::uploadMesh()
{
    const auto& device = m_vkContext.device();
    const auto& allocator = m_vkContext.allocator();

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

    VmaAllocationInfo stagingAllocationInfo = stagingBuffer.allocationInfo();
    auto& stagingData = stagingAllocationInfo.pMappedData;
    // copy vertex buffer data
    std::memcpy(stagingData, vertices.data(), vertexBufferSize);
    // copy index buffer data
    std::memcpy(static_cast<char*>(stagingData) + vertexBufferSize, indices.data(), indexBufferSize);

    auto commandBuffer = beginSingleTimeTransferCommand();
    // Record vkCmdCopyBuffer from staging buffer to device mesh vertex buffer and index buffer
    vk::BufferCopy vertexCopyRegion {.srcOffset = 0, .dstOffset = 0, .size = vertexBufferSize};
    commandBuffer->copyBuffer(stagingBuffer.buffer(), m_testMesh.vertexBuffer(), vertexCopyRegion);

    vk::BufferCopy indexCopyRegion {.srcOffset = vertexBufferSize, .dstOffset = 0, .size = indexBufferSize};
    commandBuffer->copyBuffer(stagingBuffer.buffer(), m_testMesh.indexBuffer(), indexCopyRegion);

    endSingleTimeTransferCommand(std::move(commandBuffer));
    DEBUG("Successfully uploaded mesh\n");
}

void Renderer::uploadTexture()
{
    m_testTexture = createTextureImage("../res/images/texture.jpg");
    DEBUG("Successfully uploaded texture\n");
}

void Renderer::drawFrame()
{
    const auto& device = m_vkContext.device();
    const auto& swapchain = m_vkContext.swapchain();
    const auto& frameData = m_frameData[m_frameCount % MAX_FRAMES_IN_FLIGHT];
    const auto& swapchainExtent = m_vkContext.swapchainExtent();

    const auto& frameCommandData = frameData.commandData;
    const auto& commandBuffer = *frameCommandData.commandBuffer;

    [[maybe_unused]] auto fenceRes =
        device.waitForFences(*frameCommandData.renderFence, vk::True, std::numeric_limits<uint64_t>::max());
    device.resetFences(*frameCommandData.renderFence);

    auto imgRes = device.acquireNextImageKHR(swapchain,
                                             std::numeric_limits<uint64_t>::max(),
                                             *frameCommandData.swapchainSemaphore);

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

    updateUbo(commandBuffer, frameData.ubo.buffer(), swapchainExtent);

    transitionImageLayout(commandBuffer,
                          m_vkContext.swapchainImage(imgRes.value),
                          vk::Format::eUndefined,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal);

    vk::RenderingAttachmentInfo colorAttachment {.sType = vk::StructureType::eRenderingAttachmentInfo,
                                                 .pNext = nullptr,
                                                 .imageView = m_vkContext.swapchainImageView(imgRes.value),
                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                 .resolveMode = {},
                                                 .resolveImageView = {},
                                                 .resolveImageLayout = {},
                                                 .loadOp = vk::AttachmentLoadOp::eClear,
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
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     *m_graphicsPipelineLayout,
                                     0,
                                     frameData.uboDescriptorSet,
                                     nullptr);
    commandBuffer.drawIndexed(m_testMesh.numIndices(), 1, 0, 0, 0);

    commandBuffer.endRendering();

    transitionImageLayout(commandBuffer,
                          m_vkContext.swapchainImage(imgRes.value),
                          vk::Format::eUndefined,
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
                                      .semaphore = *frameCommandData.swapchainSemaphore,
                                      .value = 1,
                                      .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                      .deviceIndex = 0};

    vk::SemaphoreSubmitInfo signalInfo {.sType = vk::StructureType::eSemaphoreSubmitInfo,
                                        .pNext = nullptr,
                                        .semaphore = *frameCommandData.renderSemaphore,
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

    m_vkContext.graphicsQueue().submit2(submitInfo2, *frameCommandData.renderFence);

    // Present
    vk::PresentInfoKHR presentInfo {.sType = vk::StructureType::ePresentInfoKHR,
                                    .pNext = nullptr,
                                    .waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &*frameCommandData.renderSemaphore,
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
