#ifndef CORE_UTILS_HPP
#define CORE_UTILS_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <filesystem>
#include <span>
#include <vector>

namespace core::utils
{

bool containsExtension(std::span<const char* const> extensionsList, const char* extensionName) noexcept;

bool containsExtension(std::span<const vk::ExtensionProperties> extensionsPropertiesList,
                       const char* extensionName) noexcept;

std::vector<char> readFile(const std::filesystem::path& path);

vk::UniqueShaderModule createShaderModuleUnique(vk::Device device, const std::vector<char>& code);

void transitionImage(vk::CommandBuffer cmd,
                     vk::Image image,
                     vk::ImageLayout currentLayout,
                     vk::ImageLayout newLayout) noexcept;

}   // namespace core::utils

#endif
