#include "Utils.hpp"

// std
#include <cstdint>
#include <fstream>

namespace renderer::utils
{

std::vector<char> readFile(const std::filesystem::path& path)
{
    std::ifstream file;
    file.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    file.open(path, std::ios::ate | std::ios::binary);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

vk::UniqueShaderModule createUniqueShaderModule(vk::Device device, const std::vector<char>& code)
{
    // vector default allocator ensures that data satisfies the alignment requirements of uint32_t
    vk::ShaderModuleCreateInfo createInfo {.sType = vk::StructureType::eShaderModuleCreateInfo,
                                           .pNext = nullptr,
                                           .flags = {},
                                           .codeSize = code.size(),
                                           .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    return device.createShaderModuleUnique(createInfo);
}

void transitionImage(vk::CommandBuffer cmd,
                     vk::Image image,
                     vk::ImageLayout currentLayout,
                     vk::ImageLayout newLayout) noexcept
{
    vk::ImageMemoryBarrier2 imageBarrier {
        .sType = vk::StructureType::eImageMemoryBarrier2,
        .pNext = nullptr,
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        .oldLayout = currentLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image = image,
        .subresourceRange = {.aspectMask = newLayout == vk::ImageLayout::eDepthAttachmentOptimal
                                               ? vk::ImageAspectFlagBits::eDepth
                                               : vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}
    };

    vk::DependencyInfo dependencyInfo {.sType = vk::StructureType::eDependencyInfo,
                                       .pNext = nullptr,
                                       .dependencyFlags = {},
                                       .memoryBarrierCount = 0,
                                       .pMemoryBarriers = nullptr,
                                       .bufferMemoryBarrierCount = 0,
                                       .pBufferMemoryBarriers = nullptr,
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &imageBarrier};

    cmd.pipelineBarrier2(dependencyInfo);
}

}   // namespace renderer::utils
