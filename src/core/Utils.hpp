#ifndef CORE_UTILS_HPP
#define CORE_UTILS_HPP

// libs
#include <vulkan/vulkan.hpp>

// std
#include <span>

namespace core::utils
{

bool isExtensionSupported(std::span<const vk::ExtensionProperties> availableExtensions,
                          const char* extensionName) noexcept;

}

#endif
