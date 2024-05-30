#ifndef CORE_UTILS_HPP
#define CORE_UTILS_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <span>

namespace core::utils
{

bool containsExtension(std::span<const char* const> extensionList, const char* extensionName) noexcept;

bool isExtensionSupported(std::span<const vk::ExtensionProperties> availableExtensions,
                          const char* extensionName) noexcept;

}   // namespace core::utils

#endif
