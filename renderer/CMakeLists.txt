get_target_property(CUNK_INCLUDE graphics INCLUDE_DIRECTORIES)

add_library(renderer INTERFACE)

set(RENDERER_LL_FILES "")

function(add_renderer_source)
    cmake_parse_arguments(PARSE_ARGV 0 PARAM "" "NAME;EXTENSION" "ARGS;INCLUDE")
    # prepare the .ll file for the runtime to eat
    list(TRANSFORM PARAM_INCLUDE PREPEND "-I")
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${PARAM_NAME}.ll COMMAND shady::vcc ARGS ${CMAKE_CURRENT_SOURCE_DIR}/${PARAM_NAME}.${PARAM_EXTENSION}  --only-run-clang ${PARAM_INCLUDE} ${PARAM_ARGS} -o ${CMAKE_BINARY_DIR}/"${PARAM_NAME}.ll" DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PARAM_NAME}.${PARAM_EXTENSION})
    add_custom_target("${PARAM_NAME}_ll" DEPENDS "${CMAKE_BINARY_DIR}/${PARAM_NAME}.ll")
    add_dependencies(vcc_rt_demo "${PARAM_NAME}_ll")
    list(APPEND RENDERER_LL_FILES "${PARAM_NAME}.ll")
    set(RENDERER_LL_FILES ${RENDERER_LL_FILES} PARENT_SCOPE)
endfunction()

add_renderer_source(NAME renderer EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize  INCLUDE ${CUNK_INCLUDE})

message("LLVM files to load at runtime: ${RENDERER_LL_FILES}")

target_link_libraries(vcc_rt_demo PRIVATE renderer)
target_compile_definitions(vcc_rt_demo PRIVATE "RENDERER_LL_FILES=${RENDERER_LL_FILES}")