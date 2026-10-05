
# The executable target
add_executable(0intro)

# Collect sources
add_subdirectory(src)

# Bake our shaders
add_subdirectory(shader)

# Link the FCG framework
target_link_libraries(0intro PRIVATE FCG-framework::Core)

# Setup include paths
target_include_directories(0intro PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
