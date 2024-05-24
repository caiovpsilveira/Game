cmake_minimum_required(VERSION 3.28) # module support

project(game VERSION 0.0.1 LANGUAGES CXX)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(game)

add_subdirectory(src)
add_subdirectory(third_party)
