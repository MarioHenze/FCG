
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <stdexcept>

// Implemented header
#include "hash_grid.h"



//////
//
// Class implementations
//

////
// std::hash<GridCell>

auto std::hash<GridCell>::operator() (const GridCell &cell) const noexcept -> std::size_t
{
	// Combine the three index hashes with the boost hash_combine recipe
	const std::hash<int> elementHash;
	std::size_t seed = elementHash(cell.x);
	seed ^= elementHash(cell.y) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
	seed ^= elementHash(cell.z) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
	return seed;
}



//////
//
// Function implementations
//

auto pointToCell (const glm::vec3 &point, const glm::vec3 &cellWidths) -> GridCell {
	return GridCell{glm::ivec3{glm::floor(point / cellWidths)}};
}

void voxelizeBox (
	const glm::vec3 &minCorner, const glm::vec3 &maxCorner, const glm::vec3 &cellWidths, std::vector<GridCell> &cells
)
{
	// Convert the corner positions to the overlapped index range
	const auto first = pointToCell(minCorner, cellWidths);
	const auto last = pointToCell(maxCorner, cellWidths);
	if (last.x < first.x || last.y < first.y || last.z < first.z)
		return;

	// Append every cell in the range
	for (int z = first.z; z <= last.z; ++z)
		for (int y = first.y; y <= last.y; ++y)
			for (int x = first.x; x <= last.x; ++x)
				cells.emplace_back(x, y, z);
}

auto cellIntersectsSphere (
	const GridCell &cell, const glm::vec3 &cellWidths, const glm::vec3 &center, float radius
) -> bool
{
	// The sphere overlaps the cell's box when its center is within radius of the closest box point
	const glm::vec3 cellMin = glm::vec3{cell.toIVec3()} * cellWidths;
	const glm::vec3 closest = glm::clamp(center, cellMin, cellMin + cellWidths);
	const glm::vec3 offset = center - closest;
	return glm::dot(offset, offset) <= radius * radius;
}

void voxelizeSphere (
	const glm::vec3 &center, float radius, const glm::vec3 &cellWidths, std::vector<GridCell> &cells
)
{
	// Enumerate the cells overlapped by the sphere's bounding box and keep the ones touching the sphere
	const glm::vec3 extent{radius};
	const auto first = pointToCell(center - extent, cellWidths);
	const auto last = pointToCell(center + extent, cellWidths);
	for (int z = first.z; z <= last.z; ++z)
		for (int y = first.y; y <= last.y; ++y)
			for (int x = first.x; x <= last.x; ++x) {
				const GridCell cell{x, y, z};
				if (cellIntersectsSphere(cell, cellWidths, center, radius))
					cells.push_back(cell);
			}
}

auto queryCuboidIndices (
	const GridCells &cells, const glm::vec3 &cellWidths, const glm::vec3 &center, const glm::vec3 &halfExtents,
	const PositionLookup &positionOf
) -> std::vector<std::size_t>
{
	//## stub: cuboid radius query
	//## gap:
	//  Broad phase: use voxelizeBox() to enumerate the grid cells overlapped by the query box (from
	//  center - halfExtents to center + halfExtents), then collect the primitive indices stored in those cells:
	//  cells maps each occupied GridCell to the indices of the primitives overlapping it, and cells.find(cell)
	//  returns cells.end() for unoccupied cells.
	//  A primitive overlapping several cells shows up once per overlapped cell, so sort the collected indices and
	//  remove the duplicates (std::sort and std::unique from <algorithm>).
	//  Narrow phase: keep only the indices whose primitive position actually lies inside the query box. The
	//  positionOf callback maps an index to its primitive's position, and a position is inside when its per-axis
	//  distance from center does not exceed halfExtents (glm::abs gives per-axis absolute values).
	//## solution
	// Broad phase: collect the primitive indices of every cell overlapped by the query box
	std::vector<GridCell> candidates;
	voxelizeBox(center - halfExtents, center + halfExtents, cellWidths, candidates);
	std::vector<std::size_t> indices;
	for (const auto &cell : candidates)
		if (const auto bucket = cells.find(cell); bucket != cells.end())
			indices.insert(indices.end(), bucket->second.begin(), bucket->second.end());

	// Deduplicate; primitives overlapping several cells appear once per overlapped cell
	std::sort(indices.begin(), indices.end());
	indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

	// Narrow phase: keep only primitives whose position lies inside the query box
	std::erase_if(indices, [&] (std::size_t index) {
		const glm::vec3 offset = glm::abs(positionOf(index) - center);
		return offset.x > halfExtents.x || offset.y > halfExtents.y || offset.z > halfExtents.z;
	});
	return indices;
	//## end
}

auto querySphereIndices (
	const GridCells &cells, const glm::vec3 &cellWidths, const glm::vec3 &center, float radius,
	const PositionLookup &positionOf
) -> std::vector<std::size_t>
{
	//## stub: spherical radius query
	//## gap:
	//  Broad phase: use voxelizeSphere() to enumerate the grid cells overlapped by the query sphere, then collect
	//  the primitive indices stored in those cells: cells maps each occupied GridCell to the indices of the
	//  primitives overlapping it, and cells.find(cell) returns cells.end() for unoccupied cells.
	//  A primitive overlapping several cells shows up once per overlapped cell, so sort the collected indices and
	//  remove the duplicates (std::sort and std::unique from <algorithm>).
	//  Narrow phase: keep only the indices whose primitive position actually lies inside the query sphere. The
	//  positionOf callback maps an index to its primitive's position, and a position is inside when its squared
	//  distance from center does not exceed the squared radius.
	//## solution
	// Broad phase: collect the primitive indices of every cell overlapped by the query sphere
	std::vector<GridCell> candidates;
	voxelizeSphere(center, radius, cellWidths, candidates);
	std::vector<std::size_t> indices;
	for (const auto &cell : candidates)
		if (const auto bucket = cells.find(cell); bucket != cells.end())
			indices.insert(indices.end(), bucket->second.begin(), bucket->second.end());

	// Deduplicate; primitives overlapping several cells appear once per overlapped cell
	std::sort(indices.begin(), indices.end());
	indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

	// Narrow phase: keep only primitives whose position lies inside the query sphere
	const float radiusSquared = radius * radius;
	std::erase_if(indices, [&] (std::size_t index) {
		const glm::vec3 offset = positionOf(index) - center;
		return glm::dot(offset, offset) > radiusSquared;
	});
	return indices;
	//## end
}
