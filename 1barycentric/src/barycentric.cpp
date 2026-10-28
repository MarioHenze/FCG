
//////
//
// Includes
//

// C++ STL
#include <stdexcept>

// Implemented header
#include "barycentric.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// \brief
/// 	Signed area of the triangle spanned by three points, positive for a counter-clockwise corner order.
///
/// Areas of triangles that share two points are the quantities whose ratios the barycentric weights are.
///
/// \param a
/// 	First corner.
/// \param b
/// 	Second corner.
/// \param c
/// 	Third corner.
///
/// \return
/// 	The signed area, in squared scene units.
[[nodiscard]] auto signedArea (const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c) -> float {
	return .5f * ((b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y));
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

auto barycentricWeights (const glm::vec2 &point, const Triangle &triangle) -> glm::vec3
{
	//## stub: barycentric weights of a point
	//## gap:
	//  Write point as w0*a + w1*b + w2*c with w0+w1+w2 == 1 and solve for the weights.
	//  Hint: each weight is the area of one of the three sub-triangles that point cuts off, divided by the area
	//  of the whole triangle. The signedArea() helper above provides those areas, and two areas share the factor
	//  1/2, so their ratio needs no square root.
	//## solution
	const auto &[a, b, c] = triangle.positions;
	const float area = signedArea(a, b, c);
	return {
		signedArea(point, b, c) / area,
		signedArea(a, point, c) / area,
		signedArea(a, b, point) / area
	};
	//## end
}

auto shadeFragment (const glm::vec2 &point, const Triangle &triangle) -> std::optional<glm::vec3>
{
	//## stub: coverage test and color interpolation
	//## gap:
	//  A fragment is part of the triangle exactly when all three of its weights are nonnegative; no separate
	//  point-in-triangle test is needed.
	//  Then blend the three corner colors with the very weights that blend the three corner positions.
	//## solution
	const glm::vec3 weights = barycentricWeights(point, triangle);
	if (weights.x < 0.f || weights.y < 0.f || weights.z < 0.f)
		return std::nullopt;

	const auto &[c0, c1, c2] = triangle.colors;
	return weights.x * c0 + weights.y * c1 + weights.z * c2;
	//## end
}
