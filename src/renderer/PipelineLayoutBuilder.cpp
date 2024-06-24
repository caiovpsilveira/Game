#include "PipelineLayoutBuilder.hpp"

namespace renderer
{

PipelineLayoutBuilder::PipelineLayoutBuilder(vk::Device device) noexcept
    : m_device(device)
{}

void PipelineLayoutBuilder::setDescriptorSetLayouts(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts)
{
    m_descriptorSetLayouts = descriptorSetLayouts;
}

vk::UniquePipelineLayout PipelineLayoutBuilder::build()
{
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {.sType = vk::StructureType::ePipelineLayoutCreateInfo,
                                                           .pNext = nullptr,
                                                           .flags = {},
                                                           .setLayoutCount =
                                                               static_cast<uint32_t>(m_descriptorSetLayouts.size()),
                                                           .pSetLayouts = m_descriptorSetLayouts.data(),
                                                           .pushConstantRangeCount = 0,
                                                           .pPushConstantRanges = nullptr};

    return m_device.createPipelineLayoutUnique(pipelineLayoutCreateInfo);
}

}   // namespace renderer
