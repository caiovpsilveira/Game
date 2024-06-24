#ifndef RENDERER_PIPELINE_LAYOUT_BUILDER
#define RENDERER_PIPELINE_LAYOUT_BUILDER

// libs
#include <vulkan/vulkan.hpp>

// std
#include <vector>

namespace renderer
{

class PipelineLayoutBuilder
{
public:
    PipelineLayoutBuilder() = delete;
    explicit PipelineLayoutBuilder(vk::Device device) noexcept;

    PipelineLayoutBuilder(const PipelineLayoutBuilder&) = delete;
    PipelineLayoutBuilder& operator=(const PipelineLayoutBuilder&) = delete;

    PipelineLayoutBuilder(PipelineLayoutBuilder&&) = delete;
    PipelineLayoutBuilder& operator=(PipelineLayoutBuilder&&) = delete;

    ~PipelineLayoutBuilder() noexcept = default;

public:
    void setDescriptorSetLayouts(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts);

    vk::UniquePipelineLayout build();

private:
    vk::Device m_device;                                           // not owned
    std::vector<vk::DescriptorSetLayout> m_descriptorSetLayouts;   // not owned
};

}   // namespace renderer

#endif
