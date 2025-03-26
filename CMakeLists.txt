cmake_minimum_required(VERSION 3.13)
project(vcc_rt_demo)

add_subdirectory(bvh)
add_subdirectory(cunk)

set(CMAKE_CXX_STANDARD 20)

find_package(glfw3 REQUIRED)
find_package(shady REQUIRED)
find_package(assimp REQUIRED)
add_executable(vcc_rt_demo main.cpp util.c blit.cpp model.cpp camera_host.cpp bvh.cpp)
target_link_libraries(vcc_rt_demo PRIVATE shady::runner shady::driver shady::api glfw graphics bvh assimp shady::nasl)

add_subdirectory(renderer)
