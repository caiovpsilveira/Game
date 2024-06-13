#ifndef RENDERER_GRAPHICS_PIPELINE_BUILDER_HPP
#define RENDERER_GRAPHICS_PIPELINE_BUILDER_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <filesystem>

namespace renderer
{

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder() = delete;
    explicit GraphicsPipelineBuilder(vk::Device device) noexcept;

    ~GraphicsPipelineBuilder() noexcept = default;

    GraphicsPipelineBuilder(const GraphicsPipelineBuilder&) = delete;
    GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&) = delete;

    GraphicsPipelineBuilder(GraphicsPipelineBuilder&&) = delete;
    GraphicsPipelineBuilder& operator=(GraphicsPipelineBuilder&&) = delete;

public:
    void setShaders(const std::filesystem::path& vertexShaderSourcePath,
                    const std::filesystem::path& fragmentShaderSourcePath);

    vk::UniquePipeline build(vk::Format swapchainColorFormat);

private:
    vk::Device m_device;   // not owned
    vk::UniqueShaderModule m_vertShaderModule;
    vk::UniqueShaderModule m_fragShaderModule;
};

}   // namespace renderer

#endif
