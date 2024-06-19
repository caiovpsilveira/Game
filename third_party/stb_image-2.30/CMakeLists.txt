project(STB_IMAGE C)
add_library(STB_IMAGE)
target_sources(STB_IMAGE PRIVATE stb_image.c)
target_include_directories(STB_IMAGE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
