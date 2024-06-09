#ifndef CORE_GRAPHICS_PIPELINE_BUILDER_HPP
#define CORE_GRAPHICS_PIPELINE_BUILDER_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <filesystem>

namespace core
{

class GraphicsPipelineBuilder
{
public:
    explicit GraphicsPipelineBuilder(vk::Device device) noexcept;

    ~GraphicsPipelineBuilder() noexcept;

    GraphicsPipelineBuilder(const GraphicsPipelineBuilder&) = delete;
    GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&) = delete;

    GraphicsPipelineBuilder(GraphicsPipelineBuilder&&) = delete;
    GraphicsPipelineBuilder& operator=(GraphicsPipelineBuilder&&) = delete;

public:
    void setShaders(const std::filesystem::path& vertexShaderSourcePath,
                    const std::filesystem::path& fragmentShaderSourcePath);

private:
    vk::Device m_device;   // not owned
    vk::ShaderModule m_vertShaderModule = nullptr;
    vk::ShaderModule m_fragShaderModule = nullptr;
};

}   // namespace core

#endif
