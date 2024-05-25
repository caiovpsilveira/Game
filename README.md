# Voxel game

This is my attempt to make a voxel game to learn Vulkan.

I'm using C++23 because of std::expected, designated initializers and other features from C++20/23, which works very well with Vulkan.

## Dependencies
For third_party libraries, I'm using [SDL2](https://www.libsdl.org/), [GLM](https://github.com/g-truc/glm) and [VulkanMemoryAllocator](https://gpuopen.com/vulkan-memory-allocator/). These are already embedded within the project.
For external dependencies, this requires the Vulkan SDK. You can download the latest Vulkan SDK [here](https://www.lunarg.com/vulkan-sdk/).

## Building
This project is configured with CMake. In-source builds are disabled.

```
mkdir build
cd build
cmake ..
make
```

This was tested with GNU and Clang.
For GNU, just setting `CMAKE_CXX_STANDARD 23` in the CMakeLists.txt worked.
For Clang, I had to add `-stdlib=libc++` to `CMAKE_CXX_FLAGS` for it to recognize std::expected, and install `libc++-dev`.
You may need to check how to enable C++23 features with your compiler.

Alternative, there is a `cmake.sh` script in the project root, which sets the compiler as Clang and overwrites some CMake rules with `CMakeOverrides.txt`, such as compile flags and `CMAKE_EXPORT_COMPILE_COMMANDS`.
