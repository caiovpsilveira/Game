#include "Utils.hpp"

namespace core::utils
{

bool isExtensionSupported(std::span<const vk::ExtensionProperties> availableExtensions,
                          const char* extensionName) noexcept
{
    auto match = [extensionName](const vk::ExtensionProperties& p) {
        return std::strcmp(p.extensionName, extensionName) == 0;
    };

    auto result = std::ranges::find_if(availableExtensions, match);
    return result != availableExtensions.end();
}

}   // namespace core::utils
