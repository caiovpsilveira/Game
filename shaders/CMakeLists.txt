find_program(GLSL_VALIDATOR glslangValidator)

set(GLSL_SOURCE_FILES simple_shader.frag simple_shader.vert)

foreach(SHADER ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV "${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
        OUTPUT ${SPIRV}
        # Does not really make much sense to output the the build directory,
        # as images will also be in the root/res/images or something like that.
        # Better to output to the root/shaders so it's consistent
        COMMAND ${GLSL_VALIDATOR} -V "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" -o "${CMAKE_CURRENT_SOURCE_DIR}/${SPIRV}"
        DEPENDS ${SHADER})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SHADER)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
