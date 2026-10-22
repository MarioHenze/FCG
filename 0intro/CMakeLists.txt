
# The "Image Viewer" demo app, built twice: "0intro" is the student scaffold that tools/elide.py generates from
# the solution, and "0intro_solution" is that solution itself. Building both keeps the scaffold honest, because
# a scaffold that elides too much stops compiling.
set(EXERCISE_SOURCES
	gamma.h
	gamma.cpp
	histogram.h
	histogram.cpp
	main.cpp
)

# A source comes from the generated tree when the generator elided something in it, and from the solution tree
# otherwise. Deriving that from the tree itself keeps this list identical for both variants, so adding a gap to
# another source needs no change here.
set(STUDENT_SOURCES "")
set(SOLUTION_SOURCES "")
foreach(source IN LISTS EXERCISE_SOURCES)
	list(APPEND SOLUTION_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/${source}")
	if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/handout/${source}")
		list(APPEND STUDENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/handout/${source}")
	else()
		list(APPEND STUDENT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/${source}")
	endif()
endforeach()

# Bake assets
cpp_embedlib_add(0intro_assets
	FILES     "${EXERCISE_ASSET_ROOT}/fcgexlogo.png"
	BASE_DIR  "${EXERCISE_ASSET_ROOT}"
	NAMESPACE intro::assets
)

# Depend on the FCG framework
add_executable(0intro          ${STUDENT_SOURCES})
add_executable(0intro_solution ${SOLUTION_SOURCES})
foreach(target IN ITEMS 0intro 0intro_solution)
	target_link_libraries(${target} PRIVATE
		FCG-Framework::Core FCG-Framework::Render FCG-Framework::Image FCG-Framework::Extras
		0intro_assets
	)

	# Headers are shared between the variants: the generator elides definitions, never declarations.
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
endforeach()

# A quote-include searches the including file's own directory first, so the scaffold keeps to its own tree.
target_include_directories(0intro PRIVATE "${CMAKE_CURRENT_LIST_DIR}/handout")
