#include "GraphicsPipelineBuilder.hpp"

#include "core/Utils.hpp"

namespace core
{

GraphicsPipelineBuilder::GraphicsPipelineBuilder(vk::Device device) noexcept
    : m_device(device)
{}

GraphicsPipelineBuilder::~GraphicsPipelineBuilder() noexcept
{
    // safe to call with nullptr, which these are initialized
    m_device.destroyShaderModule(m_vertShaderModule);
    m_device.destroyShaderModule(m_fragShaderModule);
}

void GraphicsPipelineBuilder::setShaders(const std::filesystem::path& vertexShaderSourcePath,
                                         const std::filesystem::path& fragmentShaderSourcePath)
{
    auto vertShaderCode = utils::readFile(vertexShaderSourcePath);
    auto fragShaderCode = utils::readFile(fragmentShaderSourcePath);

    m_vertShaderModule = utils::createShaderModule(m_device, vertShaderCode);
    m_fragShaderModule = utils::createShaderModule(m_device, fragShaderCode);

    // vk::PipelineShaderStageCreateInfo vertShaderStageInfo {.sType =
    // vk::StructureType::ePipelineShaderStageCreateInfo,
    //                                                        .pNext = nullptr,
    //                                                        .flags = {},
    //                                                        .stage = vk::ShaderStageFlagBits::eVertex,
    //                                                        .module = m_vertShaderModule,
    //                                                        .pName = "main",
    //                                                        .pSpecializationInfo = nullptr};

    // vk::PipelineShaderStageCreateInfo fragShaderStageInfo {.sType =
    // vk::StructureType::ePipelineShaderStageCreateInfo,
    //                                                        .pNext = nullptr,
    //                                                        .flags = {},
    //                                                        .stage = vk::ShaderStageFlagBits::eFragment,
    //                                                        .module = m_fragShaderModule,
    //                                                        .pName = "main",
    //                                                        .pSpecializationInfo = nullptr};
}

void GraphicsPipelineBuilder::setViewportAndScissor(const vk::Extent2D& swapchainExtent) noexcept
{
    m_viewport.x = 0.f;
    m_viewport.y = 0.f;
    m_viewport.width = static_cast<float>(swapchainExtent.width);
    m_viewport.height = static_cast<float>(swapchainExtent.height);
    m_viewport.minDepth = 0.f;
    m_viewport.maxDepth = 1.f;

    m_scissor.extent = {0, 0};
    m_scissor.extent = swapchainExtent;
}

}   // namespace core
