#ifndef RENDERER_DESCRIPTOR_SET_LAYOUT_BUILDER_HPP
#define RENDERER_DESCRIPTOR_SET_LAYOUT_BUILDER_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <vector>

namespace renderer
{

class DescriptorSetLayoutBuilder
{
public:
    DescriptorSetLayoutBuilder() = delete;
    explicit DescriptorSetLayoutBuilder(vk::Device device) noexcept;

    DescriptorSetLayoutBuilder(const DescriptorSetLayoutBuilder&) = delete;
    DescriptorSetLayoutBuilder& operator=(const DescriptorSetLayoutBuilder&) = delete;

    DescriptorSetLayoutBuilder(DescriptorSetLayoutBuilder&&) = delete;
    DescriptorSetLayoutBuilder& operator=(DescriptorSetLayoutBuilder&&) = delete;

    ~DescriptorSetLayoutBuilder() noexcept = default;

public:
    void addBinding(vk::DescriptorType descriptorType, uint32_t descriptorCount, vk::ShaderStageFlags stageFlags);

    vk::UniqueDescriptorSetLayout build();

private:
    vk::Device m_device;   // not owned
    std::vector<vk::DescriptorSetLayoutBinding> m_bindings;
};

}   // namespace renderer

#endif
