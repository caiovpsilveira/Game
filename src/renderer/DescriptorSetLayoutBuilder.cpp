#include "DescriptorSetLayoutBuilder.hpp"

namespace renderer
{

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(vk::Device device) noexcept
    : m_device(device)
{}

void DescriptorSetLayoutBuilder::addBinding(vk::DescriptorType descriptorType,
                                            uint32_t descriptorCount,
                                            vk::ShaderStageFlags stageFlags)
{
    uint32_t nextBindingNumber = static_cast<uint32_t>(m_bindings.size());
    vk::DescriptorSetLayoutBinding binding {.binding = nextBindingNumber,
                                            .descriptorType = descriptorType,
                                            .descriptorCount = descriptorCount,
                                            .stageFlags = stageFlags,
                                            .pImmutableSamplers = nullptr};
    m_bindings.push_back(binding);
}

vk::UniqueDescriptorSetLayout DescriptorSetLayoutBuilder::build()
{
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
        .sType = vk::StructureType::eDescriptorSetLayoutCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .bindingCount = static_cast<uint32_t>(m_bindings.size()),
        .pBindings = m_bindings.data()};

    return m_device.createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo);
}

}   // namespace renderer
