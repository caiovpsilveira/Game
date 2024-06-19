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
    void addDescriptorSetLayout(vk::UniqueDescriptorSetLayout&& descriptorSetLayout);

    vk::UniquePipelineLayout build();

private:
    vk::Device m_device;   // not owned
    std::vector<vk::UniqueDescriptorSetLayout> m_descriptorSetLayouts;
};

}   // namespace renderer

#endif
