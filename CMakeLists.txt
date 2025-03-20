cmake_minimum_required(VERSION 3.13)
project(vcc_rt_demo)

add_subdirectory(bvh)
add_subdirectory(cunk)

set(CMAKE_CXX_STANDARD 17)

find_package(glfw3 REQUIRED)
find_package(shady REQUIRED)
add_executable(vcc_rt_demo main.cpp util.c blit.cpp)
target_link_libraries(vcc_rt_demo PRIVATE shady::runner shady::driver shady::api glfw graphics bvh)

add_subdirectory(renderer)
