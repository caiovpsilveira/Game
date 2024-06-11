target_sources(game PRIVATE
               core/Logger.cpp
               core/Utils.cpp
               core/UniqueVmaAllocator.cpp
               core/VulkanGraphicsContext.cpp
               core/GraphicsPipelineBuilder.cpp
               game/Game.cpp
               main.cpp)

target_compile_definitions(game PRIVATE
                           VK_NO_PROTOTYPES
                           VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                           VMA_STATIC_VULKAN_FUNCTIONS=0
                           VMA_DYNAMIC_VULKAN_FUNCTIONS=1
                           # Below are my preferences. Since I'm not using, might as well remove to compile faster
                           VULKAN_HPP_NO_CONSTRUCTORS
                           VULKAN_HPP_NO_TO_STRING # can still be used by including vulkan_to_string.hpp
                           VULKAN_HPP_NO_SPACESHIP_OPERATOR
                           VULKAN_HPP_NO_SETTERS)

target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game PUBLIC glm SDL2)
