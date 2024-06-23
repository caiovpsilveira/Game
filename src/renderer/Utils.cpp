#include "Utils.hpp"

// std
#include <cassert>
#include <cstdint>
#include <fstream>

namespace renderer::utils
{

bool containsExtension(std::span<const char* const> extensionsList, const char* extensionName) noexcept
{
    return std::ranges::find(extensionsList, extensionName) != extensionsList.end();
}

bool containsExtension(std::span<const vk::ExtensionProperties> extensionsPropertiesList,
                       const char* extensionName) noexcept
{
    auto match = [extensionName](const vk::ExtensionProperties& p) {
        return std::strcmp(p.extensionName, extensionName) == 0;
    };

    auto result = std::ranges::find_if(extensionsPropertiesList, match);
    return result != extensionsPropertiesList.end();
}

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

vk::UniqueShaderModule createUniqueShaderModule(vk::Device device, std::span<const char> code)
{
    vk::ShaderModuleCreateInfo createInfo {.sType = vk::StructureType::eShaderModuleCreateInfo,
                                           .pNext = nullptr,
                                           .flags = {},
                                           .codeSize = code.size(),
                                           .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    return device.createShaderModuleUnique(createInfo);
}

}   // namespace renderer::utils
