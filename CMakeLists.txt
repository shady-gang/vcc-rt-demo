cmake_minimum_required(VERSION 3.13)
project(vcc_rt_demo)

set(CMAKE_CXX_STANDARD 17)

find_package(minifb REQUIRED)
find_package(shady REQUIRED)
add_executable(vcc_rt_demo main.cpp util.c)
target_link_libraries(vcc_rt_demo PRIVATE shady::runtime shady::shady minifb::minifb)

# prepare the .ll file for the runtime to eat
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/main_gpu.ll COMMAND shady::vcc ARGS ${CMAKE_CURRENT_SOURCE_DIR}/main_gpu.cpp --only-run-clang -o ${CMAKE_BINARY_DIR}/main_gpu.ll DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main_gpu.cpp)
add_custom_target(main_gpu_ll DEPENDS  ${CMAKE_BINARY_DIR}/main_gpu.ll)
add_dependencies(vcc_rt_demo main_gpu_ll)
