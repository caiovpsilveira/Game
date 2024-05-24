target_sources(game PRIVATE
               main.cpp)

target_link_libraries(game PRIVATE glm SDL2 VulkanMemoryAllocator)
