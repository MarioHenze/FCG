
# The executable target
add_executable(0intro)

# Collect sources
set(SOURCES
	main.cpp
	shapes.cpp
)

# Collect headers
set(HEADERS
	shapes.h
	shapes/convex_poly.h
	shapes/platonic_solid.h
)

# Add to our target
target_sources(0intro PRIVATE ${SOURCES})
target_sources(0intro PRIVATE ${HEADERS})

# Depend on the FCG framework
target_link_libraries(0intro PRIVATE FCG-framework::Core)

# Setup compiler
target_include_directories(0intro PRIVATE ${CMAKE_CURRENT_LIST_DIR})
