#include "GraphicsPipelineBuilder.hpp"

#include "Types.hpp"
#include "Utils.hpp"
#include "core/Logger.hpp"

// libs
#include <vulkan/vulkan_to_string.hpp>

namespace renderer
{

GraphicsPipelineBuilder::GraphicsPipelineBuilder(vk::Device device) noexcept
    : m_device(device)
{}

void GraphicsPipelineBuilder::setShaders(const std::filesystem::path& vertexShaderSourcePath,
                                         const std::filesystem::path& fragmentShaderSourcePath)
{
    auto vertShaderCode = utils::readFile(vertexShaderSourcePath);
    auto fragShaderCode = utils::readFile(fragmentShaderSourcePath);

    m_vertShaderModule = utils::createUniqueShaderModule(m_device, vertShaderCode);
    m_fragShaderModule = utils::createUniqueShaderModule(m_device, fragShaderCode);
}

void GraphicsPipelineBuilder::setPipelineLayout(vk::PipelineLayout layout) noexcept
{
    m_pipelineLayout = layout;
}

vk::UniquePipeline GraphicsPipelineBuilder::build(vk::Format swapchainColorFormat)
{
    // Shaders
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo {.sType = vk::StructureType::ePipelineShaderStageCreateInfo,
                                                           .pNext = nullptr,
                                                           .flags = {},
                                                           .stage = vk::ShaderStageFlagBits::eVertex,
                                                           .module = *m_vertShaderModule,
                                                           .pName = "main",
                                                           .pSpecializationInfo = nullptr};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo {.sType = vk::StructureType::ePipelineShaderStageCreateInfo,
                                                           .pNext = nullptr,
                                                           .flags = {},
                                                           .stage = vk::ShaderStageFlagBits::eFragment,
                                                           .module = *m_fragShaderModule,
                                                           .pName = "main",
                                                           .pSpecializationInfo = nullptr};

    vk::PipelineShaderStageCreateInfo shaderStagesCreateInfos[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    auto bindingDescription = Vertex::bindingDescription();
    auto attributeDescriptions = Vertex::attributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo {
        .sType = vk::StructureType::ePipelineVertexInputStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo {
        .sType = vk::StructureType::ePipelineInputAssemblyStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};

    // Dynamic states
    vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo {.sType =
                                                                   vk::StructureType::ePipelineDynamicStateCreateInfo,
                                                               .pNext = nullptr,
                                                               .flags = {},
                                                               .dynamicStateCount = std::size(dynamicStates),
                                                               .pDynamicStates = dynamicStates};

    // Viewport state
    vk::PipelineViewportStateCreateInfo viewportStateCreateInfo {
        .sType = vk::StructureType::ePipelineViewportStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr};

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo {
        .sType = vk::StructureType::ePipelineRasterizationStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasConstantFactor = 0.f,
        .depthBiasClamp = 0.f,
        .depthBiasSlopeFactor = 0.f,
        .lineWidth = 1.f};

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo {
        .sType = vk::StructureType::ePipelineMultisampleStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
        .minSampleShading = 1.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = vk::False,
        .alphaToOneEnable = vk::False};

    // Color Blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment {
        .blendEnable = vk::False,
        .srcColorBlendFactor = vk::BlendFactor::eOne,
        .dstColorBlendFactor = vk::BlendFactor::eZero,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo {
        .sType = vk::StructureType::ePipelineColorBlendStateCreateInfo,
        .pNext = nullptr,
        .flags = {},
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {{0.f, 0.f, 0.f, 0.f}}};

    // Rendering Create Info (dynamic rendering, Vulkan 1.3+)
    vk::PipelineRenderingCreateInfo renderingCreateInfo {.sType = vk::StructureType::ePipelineRenderingCreateInfo,
                                                         .pNext = nullptr,
                                                         .viewMask = 0,
                                                         .colorAttachmentCount = 1,
                                                         .pColorAttachmentFormats = &swapchainColorFormat,
                                                         .depthAttachmentFormat = vk::Format::eUndefined,
                                                         .stencilAttachmentFormat = vk::Format::eUndefined};

    // Graphics pipeline
    vk::GraphicsPipelineCreateInfo pipelineCreateInfo {.sType = vk::StructureType::eGraphicsPipelineCreateInfo,
                                                       .pNext = &renderingCreateInfo,
                                                       .flags = {},
                                                       .stageCount = std::size(shaderStagesCreateInfos),
                                                       .pStages = shaderStagesCreateInfos,
                                                       .pVertexInputState = &vertexInputCreateInfo,
                                                       .pInputAssemblyState = &inputAssemblyCreateInfo,
                                                       .pTessellationState = nullptr,
                                                       .pViewportState = &viewportStateCreateInfo,
                                                       .pRasterizationState = &rasterizationStateCreateInfo,
                                                       .pMultisampleState = &multisampleStateCreateInfo,
                                                       .pDepthStencilState = nullptr,
                                                       .pColorBlendState = &colorBlendStateCreateInfo,
                                                       .pDynamicState = &dynamicStateCreateInfo,
                                                       .layout = m_pipelineLayout,
                                                       .renderPass = nullptr,
                                                       .subpass = 0,
                                                       .basePipelineHandle = nullptr,
                                                       .basePipelineIndex = -1};

    auto res = m_device.createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);

    if (res.result == vk::Result::eSuccess) {
        return std::move(res.value);
    }

    FATAL_FMT("Graphics pipeline failed with result {}\n", vk::to_string(res.result));
    assert(0);
    return vk::UniquePipeline();
}

}   // namespace renderer
