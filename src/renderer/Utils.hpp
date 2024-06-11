#ifndef RENDERER_UTILS_HPP
#define RENDERER_UTILS_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <filesystem>
#include <vector>

namespace renderer::utils
{

std::vector<char> readFile(const std::filesystem::path& path);

vk::UniqueShaderModule createUniqueShaderModule(vk::Device device, const std::vector<char>& code);

void transitionImage(vk::CommandBuffer cmd,
                     vk::Image image,
                     vk::ImageLayout currentLayout,
                     vk::ImageLayout newLayout) noexcept;

}   // namespace renderer::utils

#endif
