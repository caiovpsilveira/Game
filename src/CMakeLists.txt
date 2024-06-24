# https://stackoverflow.com/questions/52135983/cmake-target-link-libraries-include-as-system-to-suppress-compiler-warnings
function(target_link_libraries_system target)
    set(options PRIVATE PUBLIC INTERFACE)
    cmake_parse_arguments(TLLS "${options}" "" "" ${ARGN})
    foreach(op ${options})
        if(TLLS_${op})
            set(scope ${op})
        endif()
    endforeach(op)
    set(libs ${TLLS_UNPARSED_ARGUMENTS})

    foreach(lib ${libs})
        get_target_property(lib_include_dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        if(lib_include_dirs)
            if(scope)
                target_include_directories(${target} SYSTEM ${scope} ${lib_include_dirs})
            else()
            target_include_directories(${target} SYSTEM PRIVATE ${lib_include_dirs})
        endif()
        else()
            message("Warning: ${lib} doesn't set INTERFACE_INCLUDE_DIRECTORIES. No include_directories set.")
        endif()
        if(scope)
            target_link_libraries(${target} ${scope} ${lib})
        else()
            target_link_libraries(${target} ${lib})
        endif()
    endforeach()
endfunction(target_link_libraries_system)

target_sources(game PRIVATE
               core/Logger.cpp
               core/UniqueVmaAllocator.cpp
               #
               renderer/VulkanGraphicsContext.cpp
               renderer/Image.cpp
               renderer/Types.cpp
               renderer/Utils.cpp
               renderer/DescriptorSetLayoutBuilder.cpp
               renderer/PipelineLayoutBuilder.cpp
               renderer/GraphicsPipelineBuilder.cpp
               renderer/Renderer.cpp
               #
               game/Game.cpp
               #
               main.cpp)

target_compile_definitions(game PRIVATE
                           VK_NO_PROTOTYPES
                           VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                           VMA_STATIC_VULKAN_FUNCTIONS=0
                           VMA_DYNAMIC_VULKAN_FUNCTIONS=1
                           GLM_FORCE_DEPTH_ZERO_TO_ONE
                           # Below are my preferences. Since I'm not using, might as well remove to compile faster
                           VULKAN_HPP_NO_CONSTRUCTORS
                           VULKAN_HPP_NO_TO_STRING # can still be used by including vulkan_to_string.hpp
                           VULKAN_HPP_NO_SPACESHIP_OPERATOR
                           VULKAN_HPP_NO_SETTERS)

target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries_system(game PUBLIC glm SDL2 STB_IMAGE)

target_compile_options(game PRIVATE -Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(game PRIVATE -Wno-nullability-extension)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(game PRIVATE -Wnoexcept -Wlogical-op -Wstrict-null-sentinel -Wzero-as-null-pointer-constant -Wuseless-cast)
endif()
