
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
