cmake_minimum_required(VERSION 3.13)
project(vcc_rt_demo)

add_subdirectory(bvh)
add_subdirectory(cunk)

set(CMAKE_CXX_STANDARD 17)

find_package(glfw3 REQUIRED)
find_package(shady REQUIRED)
add_executable(vcc_rt_demo main.cpp util.c blit.cpp)
target_link_libraries(vcc_rt_demo PRIVATE shady::runner shady::driver shady::api glfw graphics bvh)

# prepare the .ll file for the runtime to eat
get_target_property(CUNK_INCLUDE graphics INCLUDE_DIRECTORIES)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/main_gpu.ll COMMAND shady::vcc ARGS ${CMAKE_CURRENT_SOURCE_DIR}/main_gpu.cpp -std=c++20 -O3 -I${CUNK_INCLUDE} -fno-slp-vectorize -fno-vectorize --only-run-clang -o ${CMAKE_BINARY_DIR}/main_gpu.ll DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main_gpu.cpp)
add_custom_target(main_gpu_ll DEPENDS  ${CMAKE_BINARY_DIR}/main_gpu.ll)
add_dependencies(vcc_rt_demo main_gpu_ll)
