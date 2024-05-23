cmake_minimum_required(VERSION 3.15)

project(game VERSION 0.0.1 LANGUAGES CXX)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

add_subdirectory(src)
add_subdirectory(third_party)
