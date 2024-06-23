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
    void initTransferCommandData();
    void initFrameCommandData();

    void createTextureSampler();

    void createGlobalDescriptorPool();
    void allocateFrameUboBuffers();
    vk::UniqueDescriptorSetLayout createGlobalDescriptorSets();

    void createGraphicsPipeline(vk::UniqueDescriptorSetLayout&& globalSetLayout);

    vk::UniqueCommandBuffer beginSingleTimeTransferCommand();
    void endSingleTimeTransferCommand(vk::UniqueCommandBuffer&& commandBuffer);

    void transitionImageLayout(vk::CommandBuffer commandBuffer,
                               vk::Image image,
                               vk::Format format,
                               vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout);
    AllocatedTexture createTextureImage(const std::filesystem::path& path);
    void updateUbo(vk::CommandBuffer command, vk::Buffer ubo, const vk::Extent2D& swapchainExtent);

    void uploadMesh();
    void uploadTexture();

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    VulkanGraphicsContext m_vkContext;
    TransferCommandData m_transferCommandData;
    size_t m_frameCount = 0;
    FrameData m_frameData[MAX_FRAMES_IN_FLIGHT];

    vk::UniqueSampler m_textureSampler;

    vk::UniqueDescriptorPool m_globalDescriptorPool;
    vk::UniquePipelineLayout m_graphicsPipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;

    Mesh m_testMesh;
    AllocatedTexture m_testTexture;
};

}   // namespace renderer

#endif

