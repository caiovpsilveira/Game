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
    Renderer(SDL_Window* window);

public:
    void drawFrame();

private:
    void createGraphicsPipeline();
    void initTransferData();
    void initFrameData();
    void uploadMesh();

private:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VulkanGraphicsContext m_vkContext;
    vk::UniquePipeline m_graphicsPipeline;
    TransferData m_transferData;
    size_t m_frameCount = 0;
    FrameData m_frameData[MAX_FRAMES_IN_FLIGHT];
    Mesh m_testMesh;
};

}   // namespace renderer

#endif

