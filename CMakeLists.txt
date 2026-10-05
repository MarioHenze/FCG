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
	GIT_TAG           master
	OPTIONS
		"BUILD_SHARED_LIBS   OFF" # <- makes it easier to redistribute executables
		"FCG_BUILD_APP_HELLO OFF" # <- we don't need the example
)

# Add individual exercise targets
add_subdirectory(0intro)
