#include "PipelineLayoutBuilder.hpp"

namespace renderer
{

PipelineLayoutBuilder::PipelineLayoutBuilder(vk::Device device) noexcept
    : m_device(device)
{}

void PipelineLayoutBuilder::addDescriptorSetLayout(vk::UniqueDescriptorSetLayout&& descriptorSetLayout)
{
    m_descriptorSetLayouts.emplace_back(std::move(descriptorSetLayout));
}

vk::UniquePipelineLayout PipelineLayoutBuilder::build()
{
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.reserve(m_descriptorSetLayouts.size());
    std::transform(m_descriptorSetLayouts.begin(),
                   m_descriptorSetLayouts.end(),
                   std::back_inserter(descriptorSetLayouts),
                   [](const vk::UniqueDescriptorSetLayout& l) { return *l; });

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {.sType = vk::StructureType::ePipelineLayoutCreateInfo,
                                                           .pNext = nullptr,
                                                           .flags = {},
                                                           .setLayoutCount =
                                                               static_cast<uint32_t>(descriptorSetLayouts.size()),
                                                           .pSetLayouts = descriptorSetLayouts.data(),
                                                           .pushConstantRangeCount = 0,
                                                           .pPushConstantRanges = nullptr};

    return m_device.createPipelineLayoutUnique(pipelineLayoutCreateInfo);
}

}   // namespace renderer
