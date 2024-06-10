#ifndef CORE_TYPES_HPP
#define CORE_TYPES_HPP

#include <vulkan/vulkan.hpp>

namespace core
{

struct FrameData {
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandBuffer commandBuffer;
    vk::UniqueSemaphore swapchainSemaphore;
    vk::UniqueSemaphore renderSemaphore;
    vk::UniqueFence renderFence;
};

}   // namespace core

#endif
