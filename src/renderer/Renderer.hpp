#ifndef RENDERER_RENDERER_HPP
#define RENDERER_RENDERER_HPP

#include "Types.hpp"
#include "VulkanGraphicsContext.hpp"

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
    void initTransferData();
    void uploadMesh();

    void updateUbo(const AllocatedBuffer& ubo, const vk::Extent2D& swapchainExtent);

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    VulkanGraphicsContext m_vkContext;

    vk::UniqueDescriptorPool m_uboDescriptorPool;

    vk::UniquePipelineLayout m_graphicsPipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;
    TransferData m_transferData;

    size_t m_frameCount = 0;
    FrameData m_frameData[MAX_FRAMES_IN_FLIGHT];

    Mesh m_testMesh;
};

}   // namespace renderer

#endif

