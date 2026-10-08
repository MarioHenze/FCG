
# The "Image Viewer" demo app.
add_executable(0intro)

# Depend on the FCG framework
target_link_libraries(0intro PRIVATE Core Image Render)

# Collect sources
add_subdirectory(src)

# Setup compiler
target_include_directories(0intro PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
