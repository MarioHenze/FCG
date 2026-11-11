
//////
//
// Includes
//

// C++ STL
#include <stdexcept>

// Local includes
#include "hash_grid.h"

// Implemented header
#include "primitives.h"



//////
//
// Class implementations
//

////
// SpherePrimitive

auto SpherePrimitive::intersectsSphere (const glm::vec3 &center, float radius) const -> bool
{
	const glm::vec3 offset = position - center;
	const float combinedRadius = this->radius + radius;
	return glm::dot(offset, offset) <= combinedRadius * combinedRadius;
}

auto SpherePrimitive::intersectsBox (const glm::vec3 &minCorner, const glm::vec3 &maxCorner) const -> bool
{
	// A sphere overlaps the box when its center is within radius of the closest box point
	const glm::vec3 closest = glm::clamp(position, minCorner, maxCorner);
	const glm::vec3 offset = position - closest;
	return glm::dot(offset, offset) <= radius * radius;
}

void SpherePrimitive::voxelize (const glm::vec3 &cellWidths, std::vector<GridCell> &cells) const
{
	// Enumerate the sphere's bounding-box cells and accept each cell through the primitive's box test
	const glm::vec3 extent{radius};
	const auto first = pointToCell(position - extent, cellWidths);
	const auto last = pointToCell(position + extent, cellWidths);
	for (int z = first.z; z <= last.z; ++z)
		for (int y = first.y; y <= last.y; ++y)
			for (int x = first.x; x <= last.x; ++x) {
				const GridCell cell{x, y, z};
				const glm::vec3 cellMin = glm::vec3{cell.toIVec3()} * cellWidths;
				if (intersectsBox(cellMin, cellMin + cellWidths))
					cells.push_back(cell);
			}
}


////
// VertexPrimitive

VertexPrimitive::VertexPrimitive (const OpenMesh::SmartVertexHandle &vh)
	: VertexPrimitive(*static_cast<const fcg::Mesh*>(vh.mesh()), vh) {}

VertexPrimitive::VertexPrimitive (const fcg::Mesh &mesh, OpenMesh::VertexHandle vh) : handle(vh)
{
	// Reject dangling handles outright
	if (!vh.is_valid())
		throw std::invalid_argument("Vertex primitive requires a valid vertex handle");

	// Copy the vertex position
	const auto &point = mesh.point(vh);
	position = {point[0], point[1], point[2]};

	// Read the sphere radius when the mesh provides it
	OpenMesh::VPropHandleT<float> property;
	if (mesh.get_property_handle(property, "v:radius"))
		radius = mesh.property(property, vh);
}


static_assert(Primitive<SpherePrimitive>);
static_assert(Primitive<VertexPrimitive>);
