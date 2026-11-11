
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <stdexcept> // std::logic_error in the generated handout's TODO stubs

// Local includes
#include "primitives.h"

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
){
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
) -> bool {
	const glm::vec3 cellMin = glm::vec3{cell.toIVec3()} * cellWidths;
	return SpherePrimitive{center, radius}.intersectsBox(cellMin, cellMin + cellWidths);
}

void voxelizeSphere (
	const glm::vec3 &center, float radius, const glm::vec3 &cellWidths, std::vector<GridCell> &cells
){
	SpherePrimitive{center, radius}.voxelize(cellWidths, cells);
}

// Collect the deduplicated indices of the primitives intersecting an axis-aligned query box.
//
// `grid` – The grid's occupied cells.
// `cellWidths` – The grid's cell extents per axis.
// `center` – Box center in world coordinates.
// `halfExtents` – Finite, nonnegative per-axis half-extents of the query box, in world units.
// `lookupPrimitive` – Callable retrieving a primitive via its index.
//
// Return the matching primitive indices, each occurring once.
template <Primitive P>
auto queryCuboidIndices (
	const GridCells &grid, const glm::vec3 &cellWidths, const glm::vec3 &center, const glm::vec3 &halfExtents,
	const PrimitiveLookup<P> &lookupPrimitive
) -> std::vector<std::size_t>
{
	//## stub: cuboid radius query
	//## gap:
	//  Broad phase:
	//  Use `voxelizeBox()` to enumerate the grid cells overlapped by the query box (from `center-halfExtents` to
	//  `center+halfExtents`), then collect the primitive indices stored in those cells:
	//  Each occupied `GridCell` maps to the indices of overlapping primitives, and `grid.find(cell)` returns
	//  `grid.end()` for unoccupied cells.
	//  A primitive overlapping several cells shows up once per overlapped cell, so deduplication is required. An
	//  idiomatic way to do this in C++ for small lists is to sort the collected indices via `std::sort()` and then
	//  remove all duplicates by first filtering via `std::unique()` and then pruning the tail via
	//  `std::vector::erase()` (`sort` and `unique` from `#include <algorithm>`). You may use any other method however,
	//  including hand-written loops or accumulating unique indices in an `std::set`.
	//
	//  Narrow phase:
	//  Keep only the indices whose primitive intersects the query box, including those that just touch its boundary.
	//  The `lookupPrimitive` callable fetches you the primitive for a given index; call its `intersectsBox()` method
	//  with the query box to decide if it needs to be dropped from the list you return.
	//## solution
	// Broad phase: collect the primitive indices of every cell overlapped by the query box
	const glm::vec3 minCorner = center - halfExtents;
	const glm::vec3 maxCorner = center + halfExtents;
	std::vector<GridCell> candidates;
	voxelizeBox(minCorner, maxCorner, cellWidths, candidates);
	std::vector<std::size_t> indices;
	for (const auto &cell : candidates)
		if (const auto bucket = grid.find(cell); bucket != grid.end())
			indices.insert(indices.end(), bucket->second.begin(), bucket->second.end());

	// Deduplicate: primitives overlapping several cells might appear several times, once per overlapped cell
	// - sort the collected indices, so duplicates will show up next to each other
	std::sort(indices.begin(), indices.end());
	// - in-place remove all duplicates and compact the vector elements, remembering the first now-unused slot in the
	//   vector
	const auto firstUnusedItem = std::unique(indices.begin(), indices.end());
	// - prune the vector tail, which is as long as there were duplicates, and now likely contains garbage
	indices.erase(firstUnusedItem, indices.end());

	// Narrow phase: keep only primitives intersecting the query box
	std::erase_if(indices, [&](std::size_t index) {
		return !lookupPrimitive(index).intersectsBox(minCorner, maxCorner);
	});
	return indices;
	//## end
}

// Collect the deduplicated indices of the primitives intersecting a query sphere.
//
// `grid` – The grid's occupied cells.
// `cellWidths` – The grid's cell extents per axis.
// `center` – Sphere center in world coordinates.
// `radius` – Finite, nonnegative query radius in world units.
// `lookupPrimitive` – Callable retrieving a primitive via its index.
//
// Return the matching primitive indices, each occurring once.
template <Primitive P>
auto querySphereIndices (
	const GridCells &grid, const glm::vec3 &cellWidths, const glm::vec3 &center, float radius,
	const PrimitiveLookup<P> &lookupPrimitive
) -> std::vector<std::size_t>
{
	//## stub: spherical radius query
	//## gap:
	//  Broad phase:
	//  Use `voxelizeSphere()` to enumerate the grid cells overlapped by the query sphere, then collect the primitive
	//  indices stored in those cells:
	//  Each occupied `GridCell` maps to the indices of overlapping primitives, and `grid.find(cell)` returns
	//  `grid.end()` for unoccupied cells.
	//  A primitive overlapping several cells shows up once per overlapped cell, so deduplication is required. An
	//  idiomatic way to do this in C++ for small lists is to sort the collected indices via `std::sort()` and then
	//  remove all duplicates by first filtering via `std::unique()` and then pruning the tail via
	//  `std::vector::erase()` (`sort` and `unique` from `#include <algorithm>`). You may use any other method however,
	//  including hand-written loops or accumulating unique indices in an `std::set`.
	//
	//  Narrow phase:
	//  Keep only the indices whose primitive intersects the query sphere, including those that just touch its boundary.
	//  The `lookupPrimitive` callable fetches you the primitive for a given index; call its `intersectsSphere()` method
	//  with the query sphere to decide if it needs to be dropped from the list you return.
	//## solution
	// Broad phase: collect the primitive indices of every cell overlapped by the query sphere
	std::vector<GridCell> candidates;
	voxelizeSphere(center, radius, cellWidths, candidates);
	std::vector<std::size_t> indices;
	for (const auto &cell : candidates)
		if (const auto bucket = grid.find(cell); bucket != grid.end())
			indices.insert(indices.end(), bucket->second.begin(), bucket->second.end());

	// Deduplicate: primitives overlapping several cells might appear several times, once per overlapped cell
	// - sort the collected indices, so duplicates will show up next to each other
	std::sort(indices.begin(), indices.end());
	// - in-place remove all duplicates and compact the vector elements, remembering the first now-unused slot in the
	//   vector
	const auto firstUnusedItem = std::unique(indices.begin(), indices.end());
	// - prune the vector tail, which is as long as there were duplicates, and now likely contains garbage
	indices.erase(firstUnusedItem, indices.end());

	// Narrow phase: keep only primitives intersecting the query sphere
	std::erase_if(indices, [&](std::size_t index) {
		return !lookupPrimitive(index).intersectsSphere(center, radius);
	});
	return indices;
	//## end
}



//////
//
// Explicit template instantiations
//

// Instantiate once per primitive type known to us, independently of the HashGrid parent implementation. Using the
// exercise code with any other primitive would require adding the corresponding instantiations here.
// Normally these two template functions would be defined in the header making this unnecessary, but this facilitates
// self-contained handouts.
template auto queryCuboidIndices<SpherePrimitive> (
	const GridCells&, const glm::vec3&, const glm::vec3&, const glm::vec3&, const PrimitiveLookup<SpherePrimitive>&
) -> std::vector<std::size_t>;

template auto querySphereIndices<SpherePrimitive> (
	const GridCells&, const glm::vec3&, const glm::vec3&, float, const PrimitiveLookup<SpherePrimitive>&
) -> std::vector<std::size_t>;

template auto queryCuboidIndices<VertexPrimitive> (
	const GridCells&, const glm::vec3&, const glm::vec3&, const glm::vec3&, const PrimitiveLookup<VertexPrimitive>&
) -> std::vector<std::size_t>;

template auto querySphereIndices<VertexPrimitive> (
	const GridCells&, const glm::vec3&, const glm::vec3&, float, const PrimitiveLookup<VertexPrimitive>&
) -> std::vector<std::size_t>;
