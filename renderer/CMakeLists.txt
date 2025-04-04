get_target_property(NASL_INCLUDE shady::nasl INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(BVH_INCLUDE bvh INTERFACE_INCLUDE_DIRECTORIES)

add_library(renderer INTERFACE)

set(RENDERER_SRC_FILES "")
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
    list(APPEND RENDERER_SRC_FILES "${PARAM_NAME}.${PARAM_EXTENSION}")
    set(RENDERER_SRC_FILES ${RENDERER_SRC_FILES} PARENT_SCOPE)
endfunction()

add_renderer_source(NAME renderer EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize  INCLUDE ${NASL_INCLUDE})
add_renderer_source(NAME camera EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize  INCLUDE ${NASL_INCLUDE})
add_renderer_source(NAME primitives EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize  INCLUDE ${NASL_INCLUDE})
#add_renderer_source(NAME bvh EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize INCLUDE ${NASL_INCLUDE} ${BVH_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR} "/home/hugo/git/llvm-project-18.1.0rc2.src/libcxx/include/")
add_renderer_source(NAME bvh EXTENSION cpp ARGS --std=c++20 -O3 -fno-slp-vectorize -fno-vectorize INCLUDE ${NASL_INCLUDE})

list(JOIN RENDERER_LL_FILES ":" RENDERER_LL_FILES_SEMI)
message("LLVM files to load at runtime: ${RENDERER_LL_FILES_SEMI}")

add_library(renderer_host STATIC ${RENDERER_SRC_FILES})
target_link_libraries(renderer_host PRIVATE shady::nasl)
target_link_libraries(renderer_host PRIVATE bvh)
target_include_directories(renderer_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vcc_rt_demo PRIVATE renderer renderer_host)
#target_link_libraries(vcc_rt_demo PRIVATE renderer)
target_compile_definitions(vcc_rt_demo PRIVATE "RENDERER_LL_FILES=${RENDERER_LL_FILES_SEMI}")