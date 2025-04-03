cmake_minimum_required(VERSION 3.13)
project(ra)

add_subdirectory(bvh)
add_subdirectory(imr)

set(CMAKE_CXX_STANDARD 20)

find_package(glfw3 REQUIRED)
find_package(shady REQUIRED)
find_package(assimp REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(driver)
add_subdirectory(renderer)
