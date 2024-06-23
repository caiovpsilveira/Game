#ifndef RENDERER_RENDERER_HPP
#define RENDERER_RENDERER_HPP

#include "Types.hpp"
#include "VulkanGraphicsContext.hpp"

// std
#include <filesystem>

struct SDL_Window;

namespace renderer
{

class Renderer
{
public:
    Renderer() noexcept = default;
    explicit Renderer(SDL_Window* window);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;

    ~Renderer() noexcept = default;

public:
    void drawFrame();

private:
    void initFrameCommandData();
    void createUboDescriptorPool();
    void allocateFrameUboBuffers();
    vk::UniqueDescriptorSetLayout createUbosDescriptorSets();

    void createGraphicsPipeline(vk::UniqueDescriptorSetLayout&& uboSetLayout);
    void initTransferCommandData();

    vk::UniqueCommandBuffer beginSingleTimeTransferCommand();
    void endSingleTimeTransferCommand(vk::UniqueCommandBuffer&& commandBuffer);

    void uploadMesh();
    void transitionImageLayout(vk::CommandBuffer commandBuffer,
                               vk::Image image,
                               vk::Format format,
                               vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout);
    Allocated2DImage createTextureImage(const std::filesystem::path& path);
    void updateUbo(vk::CommandBuffer command, vk::Buffer ubo, const vk::Extent2D& swapchainExtent);

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    VulkanGraphicsContext m_vkContext;

    vk::UniqueDescriptorPool m_uboDescriptorPool;

    vk::UniquePipelineLayout m_graphicsPipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;
    TransferCommandData m_transferCommandData;

    size_t m_frameCount = 0;
    FrameData m_frameData[MAX_FRAMES_IN_FLIGHT];

    Mesh m_testMesh;
};

}   // namespace renderer

#endif

