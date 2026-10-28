
#ifndef __1BARYCENTRIC_BARYCENTRIC_H__

#define __1BARYCENTRIC_BARYCENTRIC_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <optional>

// GLM library
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>



//////
//
// Structs and enums
//

/// \brief
/// 	A triangle in the XY plane with one color attached to each of its vertices.
///
/// Positions are given in scene units, where one unit is the edge length of one fragment of the exercise's
/// framebuffer. The vertex order is the winding order of the triangle; it does not affect the result.
///
/// The vertex colors are linear light values, i.e. the space in which mixing them is physically meaningful. The
/// display applies its gamma to the mixed result afterwards, so a color that looks like a display code in a color
/// picker has to be decoded before it belongs here.
struct Triangle
{
	/// The positions of the vertices in the XY plane.
	std::array<glm::vec2, 3> positions{};

	/// The colors at the vertices, one per entry of \ref positions.
	std::array<glm::vec3, 3> colors{};
};



//////
//
// Functions
//

/// \brief
/// 	Computes the barycentric weights of a point with respect to a triangle.
///
/// The weights express \p point as <tt>w0 * positions[0] + w1 * positions[1] + w2 * positions[2]</tt> with
/// <tt>w0 + w1 + w2 == 1</tt>. Each weight belongs to the vertex of the same index and measures how close the
/// point is to that vertex: it is 1 at the vertex itself, 0 along the opposite edge, and nonnegative everywhere
/// inside the triangle.
///
/// \pre
/// 	The three vertices of \p triangle are not collinear.
///
/// \param point
/// 	The point to express, typically the center of a fragment.
/// \param triangle
/// 	The triangle whose vertices the weights refer to.
///
/// \return
/// 	The weights (w0, w1, w2). They are not clamped, so a point outside the triangle yields at least one
/// 	negative weight.
[[nodiscard]] auto barycentricWeights (const glm::vec2 &point, const Triangle &triangle) -> glm::vec3;

/// \brief
/// 	Computes the color of one fragment of a triangle whose vertices carry different colors.
///
/// This is the per-fragment step of rasterization: for every fragment the rasterizer covers, it asks for the
/// fragment's color, given the fragment's center and the vertices of the triangle being drawn.
///
/// \pre
/// 	The three vertices of \p triangle are not collinear.
///
/// \param point
/// 	The center of the fragment to shade.
/// \param triangle
/// 	The triangle the fragment belongs to.
///
/// \return
/// 	The RGB color of the fragment, or \c std::nullopt if \p point lies outside the triangle.
[[nodiscard]] auto shadeFragment (const glm::vec2 &point, const Triangle &triangle) -> std::optional<glm::vec3>;


#endif // ifndef __1BARYCENTRIC_BARYCENTRIC_H__
