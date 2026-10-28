# The "Barycentric interpolation" exercise, built twice: "1barycentric" is the student scaffold that tools/elide.py
# generates from the solution, and "1barycentric_solution" is that solution itself. Building both keeps the scaffold
# honest, because a scaffold that elides too much stops compiling.
set(EXERCISE_SOURCES
	barycentric.h
	barycentric.cpp
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
add_executable(1barycentric          ${STUDENT_SOURCES})
add_executable(1barycentric_solution ${SOLUTION_SOURCES})
foreach(target IN ITEMS 1barycentric 1barycentric_solution)
	target_link_libraries(${target} PRIVATE FCG-Framework::Core FCG-Framework::Render)

	# Headers are shared between the variants: the generator elides definitions, never declarations.
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
endforeach()

# A quote-include searches the including file's own directory first, so the scaffold keeps to its own tree.
target_include_directories(1barycentric PRIVATE "${CMAKE_CURRENT_LIST_DIR}/handout")
