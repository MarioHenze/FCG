cmake_minimum_required(VERSION 3.31)

# Setup CMake project
project(FCG)

# Pre-configuration
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include CPM for dependency fetching
include(cmake/CPM.cmake)

# Fetch FCG-Framework
CPMFindPackage(
	NAME              FCG-Framework
	GITHUB_REPOSITORY brussig-tud/FCG-Framework
	GIT_TAG           adfaa24a4a5737a54fbcccd59f92bb38e07594a2
	OPTIONS
		"BUILD_SHARED_LIBS OFF" # <- makes it easier to redistribute executables
		"FCG_BUILD_APPS    OFF" # <- we don't need the example apps
		"SDLIMAGE_AVIF     ON"  # <- enable AVIF decoding in the SDL_image backend
		"SDLIMAGE_STRICT   ON"  # <- fail configuration if our selected decoders can not be enabled
)

# Make sure we will definitely have access to cpp-embedlib
if(NOT COMMAND cpp_embedlib_add)
	message("FCG-Framework did not expose cpp-embedlib, fetching from github")
	CPMAddPackage(
		NAME              cpp-embedlib
		GITHUB_REPOSITORY yhirose/cpp-embedlib
		GIT_TAG           main
	)
endif()

# Make sure subprojects know where the assets are
cmake_path(SET EXERCISE_ASSET_ROOT NORMALIZE "${CMAKE_CURRENT_LIST_DIR}/assets")

# Add individual exercise targets
add_subdirectory(0intro)
