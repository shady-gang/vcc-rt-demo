add_executable(ra main.cpp util.c driver.cpp model.cpp camera_host.cpp bvh_host.cpp)
target_link_libraries(ra PRIVATE Vulkan::Vulkan glfw imr)
target_link_libraries(ra PRIVATE bvh)
target_link_libraries(ra PRIVATE assimp::assimp)
target_link_libraries(ra PRIVATE shady::runner shady::driver)