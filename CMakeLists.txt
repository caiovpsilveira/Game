cmake_minimum_required(VERSION 3.25)

project(game VERSION 0.0.1 LANGUAGES CXX)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable -stdlib=libc++ for Clang to be able to find std::format
# Also disable Clang nullability extension warnings, because VMA floods with them
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

add_executable(game)

add_subdirectory(third_party)
add_subdirectory(shaders)
add_subdirectory(src)

add_dependencies(game shaders)
