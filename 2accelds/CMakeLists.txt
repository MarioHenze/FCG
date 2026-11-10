# The "Acceleration data structures" exercise, built twice: "2accelds" is the student scaffold that tools/elide.py
# generates from the solution, and "2accelds_solution" is that solution itself. Building both keeps the scaffold
# honest, because a scaffold that elides too much stops compiling.
set(EXERCISE_SOURCES
	camera.h
	camera.cpp
	first_person_camera.h
	first_person_camera.cpp
	hash_grid.h
	hash_grid.cpp
	query_cursor.h
	triangle_soup.h
	triangle_soup.cpp
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

# Depend on the FCG framework
add_executable(2accelds          ${STUDENT_SOURCES})
add_executable(2accelds_solution ${SOLUTION_SOURCES})
foreach(target IN ITEMS 2accelds 2accelds_solution)
	target_link_libraries(${target} PRIVATE
		FCG-Framework::Core FCG-Framework::Image FCG-Framework::Mesh
		FCG-Framework::Render FCG-Framework::Extras
	)

	# Headers are shared between the variants: the generator elides definitions, never declarations.
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
endforeach()

# A quote-include searches the including file's own directory first, so the scaffold keeps to its own tree.
target_include_directories(2accelds PRIVATE "${CMAKE_CURRENT_LIST_DIR}/handout")
