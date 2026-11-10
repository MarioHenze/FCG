
#ifndef __2ACCELDS_HASH_GRID_H__
#define __2ACCELDS_HASH_GRID_H__


//////
//
// Includes
//

// C++ STL
#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

// GLM library
#include <glm/glm.hpp>



//////
//
// Structs and enums
//

/// A signed 3D index identifying one cell of a uniform spatial grid.
struct GridCell
{
	/// Cell index along the X axis.
	int x = 0;

	/// Cell index along the Y axis.
	int y = 0;

	/// Cell index along the Z axis.
	int z = 0;

	/// The cell at the origin.
	constexpr GridCell () = default;

	/// \brief The cell with the given per-axis indices.
	///
	/// \param x Cell index along the X axis.
	/// \param y Cell index along the Y axis.
	/// \param z Cell index along the Z axis.
	constexpr GridCell (int x, int y, int z) noexcept : x(x), y(y), z(z) {}

	/// \brief The cell with the given per-axis indices.
	///
	/// \param v The per-axis cell indices.
	constexpr explicit GridCell (const glm::ivec3 &v) noexcept : x(v.x), y(v.y), z(v.z) {}

	/// The cell indices as a GLM vector.
	[[nodiscard]] constexpr auto toIVec3 () const noexcept -> glm::ivec3 {
		return {x, y, z};
	}

	/// Cells compare equal exactly when all three indices match.
	[[nodiscard]] constexpr auto operator== (const GridCell&) const noexcept -> bool = default;
};

/// std::hash support for GridCell, enabling its use as an unordered_map key.
template <>
struct std::hash<GridCell>
{
	/// \brief Combine the three cell indices into one hash value.
	///
	/// \param cell The cell to hash.
	///
	/// \return A hash value; equal cells hash equally.
	[[nodiscard]] auto operator() (const GridCell &cell) const noexcept -> std::size_t;
};

/// Map from an occupied grid cell to the indices of the primitives overlapping it.
using GridCells = std::unordered_map<GridCell, std::vector<std::size_t>>;

/// Callback returning the world-space position of the primitive with a given index.
using PositionLookup = std::function<glm::vec3 (std::size_t)>;

/// Smallest permitted grid cell width per axis, keeping the cell index conversion well-defined.
inline constexpr float minCellWidth = 1e-5f;



//////
//
// Concepts
//

/// \brief Types that a HashGrid can index spatially.
///
/// A primitive exposes a world-space \c position and a \c voxelize method appending every grid cell the primitive
/// overlaps to a vector.
template <typename T>
concept Primitive = requires (const T &primitive, const glm::vec3 &cellWidths, std::vector<GridCell> &cells) {
	{ primitive.position } -> std::convertible_to<glm::vec3>;
	{ primitive.voxelize(cellWidths, cells) } -> std::same_as<void>;
};

/// \brief Random-access containers from which a HashGrid can read primitives by index.
///
/// The container's elements must be addressable in constant time by index and must be convertible into the grid's
/// primitive type, which allows zero-copy views over externally owned data.
template <typename C, typename P>
concept PrimitiveStore =
	std::ranges::random_access_range<const C> &&
	std::ranges::sized_range<const C> &&
	std::constructible_from<P, std::ranges::range_reference_t<const C>>;



//////
//
// Functions
//

/// \brief Compute the index of the grid cell containing a world-space point.
///
/// \param point The world-space point.
/// \param cellWidths Grid cell extents; must be positive per axis.
///
/// \return The containing cell's index; negative coordinates map to negative indices.
[[nodiscard]] auto pointToCell (const glm::vec3 &point, const glm::vec3 &cellWidths) -> GridCell;

/// \brief Append every grid cell overlapped by an axis-aligned box.
///
/// \param minCorner The box's minimum corner in world coordinates.
/// \param maxCorner The box's maximum corner in world coordinates; when below \p minCorner along an axis, nothing is
/// 	appended.
/// \param cellWidths Grid cell extents; must be positive per axis.
/// \param cells Receives the overlapped cells, appended in lexicographic Z-Y-X order.
void voxelizeBox (
	const glm::vec3 &minCorner, const glm::vec3 &maxCorner, const glm::vec3 &cellWidths,
	std::vector<GridCell> &cells
);

/// \brief Test whether a grid cell's box overlaps a sphere.
///
/// \param cell The cell to test.
/// \param cellWidths Grid cell extents; must be positive per axis.
/// \param center Sphere center in world coordinates.
/// \param radius Sphere radius in world units; touching the cell's box counts as overlapping.
///
/// \return True when the sphere and the cell's box overlap.
[[nodiscard]] auto cellIntersectsSphere (
	const GridCell &cell, const glm::vec3 &cellWidths, const glm::vec3 &center, float radius
) -> bool;

/// \brief Append every grid cell overlapped by a sphere.
///
/// \param center Sphere center in world coordinates.
/// \param radius Sphere radius in world units; zero selects the single cell containing \p center.
/// \param cellWidths Grid cell extents; must be positive per axis.
/// \param cells Receives the overlapped cells, appended in lexicographic Z-Y-X order.
void voxelizeSphere (
	const glm::vec3 &center, float radius, const glm::vec3 &cellWidths, std::vector<GridCell> &cells
);

/// \brief Collect the deduplicated indices of the primitives whose position lies inside an axis-aligned query box.
///
/// In the broad phase, the primitive indices of all grid cells overlapped by the box are gathered and deduplicated;
/// in the narrow phase, each candidate's position is tested against the box exactly.
///
/// \param cells The grid's occupied cells mapped to their primitive indices.
/// \param cellWidths The grid's cell extents per axis.
/// \param center Box center in world coordinates.
/// \param halfExtents Box half-extents per axis in world units.
/// \param positionOf Maps a primitive index to its world-space position.
///
/// \return The matching primitive indices, sorted ascending, each occurring once.
[[nodiscard]] auto queryCuboidIndices (
	const GridCells &cells, const glm::vec3 &cellWidths, const glm::vec3 &center, const glm::vec3 &halfExtents,
	const PositionLookup &positionOf
) -> std::vector<std::size_t>;

/// \brief Collect the deduplicated indices of the primitives whose position lies inside a query sphere.
///
/// In the broad phase, the primitive indices of all grid cells overlapped by the sphere are gathered and
/// deduplicated; in the narrow phase, each candidate's position is tested against the sphere exactly.
///
/// \param cells The grid's occupied cells mapped to their primitive indices.
/// \param cellWidths The grid's cell extents per axis.
/// \param center Sphere center in world coordinates.
/// \param radius Sphere radius in world units.
/// \param positionOf Maps a primitive index to its world-space position.
///
/// \return The matching primitive indices, sorted ascending, each occurring once.
[[nodiscard]] auto querySphereIndices (
	const GridCells &cells, const glm::vec3 &cellWidths, const glm::vec3 &center, float radius,
	const PositionLookup &positionOf
) -> std::vector<std::size_t>;



//////
//
// Classes
//

/// \brief A uniform spatial hash grid indexing primitives by the cells they overlap.
///
/// The grid maps each occupied GridCell to the indices of the primitives overlapping it and answers cuboid and
/// spherical radius queries by visiting only overlapped cells. Primitives are read by index from a caller-supplied
/// random-access store, which may be an owning container or a zero-copy view of externally owned data. Rebuilding
/// with new cell widths re-voxelizes the stored primitives; the store itself is left untouched.
///
/// \tparam P The indexed primitive type, satisfying the Primitive concept.
/// \tparam Container The primitive store, satisfying the PrimitiveStore concept for \c P.
template <Primitive P, PrimitiveStore<P> Container = std::vector<P>>
class HashGrid
{
public:

	////
	// Types

	/// Map from an occupied grid cell to the indices of the primitives overlapping it.
	using MapType = GridCells;


	////
	// Object construction

	/// An empty grid with unit cell widths.
	HashGrid () = default;

	/// \brief Index the given primitives with the given cell widths.
	///
	/// Cell widths are clamped to at least \c minCellWidth per axis, so zero or negative widths cannot occur.
	///
	/// \param primitives The primitive store to index; moved into the grid.
	/// \param cellWidths The grid cell extents per axis in world units.
	HashGrid (Container primitives, const glm::vec3 &cellWidths) : m_primitives(std::move(primitives)) {
		rebuild(cellWidths);
	}


	////
	// Accessors

	/// The cell widths the grid was built with.
	[[nodiscard]] auto cellWidths () const noexcept -> const glm::vec3& {
		return m_cellWidths;
	}

	/// The indexed primitive store.
	[[nodiscard]] auto primitives () const noexcept -> const Container& {
		return m_primitives;
	}

	/// Number of indexed primitives.
	[[nodiscard]] auto primitiveCount () const noexcept -> std::size_t {
		return std::ranges::size(m_primitives);
	}

	/// \brief Read one indexed primitive.
	///
	/// \param index The primitive index, below primitiveCount().
	///
	/// \return The primitive, constructed from the store's element at \p index.
	[[nodiscard]] auto primitive (std::size_t index) const -> P {
		return P{m_primitives[index]};
	}

	/// The occupied cells mapped to their primitive indices.
	[[nodiscard]] auto cells () const noexcept -> const MapType& {
		return m_cells;
	}

	/// Number of occupied cells.
	[[nodiscard]] auto occupiedCellCount () const noexcept -> std::size_t {
		return m_cells.size();
	}

	/// \brief Find the bucket of one cell.
	///
	/// \param cell The cell to look up.
	///
	/// \return Iterator to the cell's bucket, or cells().end() when the cell is unoccupied.
	[[nodiscard]] auto find (const GridCell &cell) const -> MapType::const_iterator {
		return m_cells.find(cell);
	}

	/// \brief Read the primitive indices stored in one cell.
	///
	/// \param cell The cell to look up.
	///
	/// \return The primitive indices overlapping \p cell; empty when the cell is unoccupied.
	[[nodiscard]] auto operator[] (const GridCell &cell) const -> std::span<const std::size_t> {
		const auto bucket = m_cells.find(cell);
		return bucket != m_cells.end() ? std::span<const std::size_t>{bucket->second} : std::span<const std::size_t>{};
	}


	////
	// Methods

	/// \brief Re-voxelize every primitive into the grid with new cell widths.
	///
	/// Cell widths are clamped to at least \c minCellWidth per axis, so zero or negative widths cannot occur. The
	/// primitive store is left untouched.
	///
	/// \param cellWidths The new grid cell extents per axis in world units.
	void rebuild (const glm::vec3 &cellWidths)
	{
		// Clamp to strictly positive widths to keep the cell index conversion well-defined
		m_cellWidths = glm::max(cellWidths, glm::vec3{minCellWidth});

		// Voxelize each primitive and record its index in every overlapped cell
		m_cells.clear();
		std::vector<GridCell> overlapped;
		for (std::size_t index = 0, count = std::ranges::size(m_primitives); index < count; ++index) {
			overlapped.clear();
			primitive(index).voxelize(m_cellWidths, overlapped);
			for (const auto &cell : overlapped)
				m_cells[cell].push_back(index);
		}
	}

	/// \brief Collect the deduplicated indices of the primitives whose position lies inside an axis-aligned query
	/// 	box.
	///
	/// \param center Box center in world coordinates.
	/// \param halfExtents Box half-extents per axis in world units.
	///
	/// \return The matching primitive indices, sorted ascending, each occurring once.
	[[nodiscard]] auto queryCuboidIndices (const glm::vec3 &center, const glm::vec3 &halfExtents) const
		-> std::vector<std::size_t> {
		return ::queryCuboidIndices(m_cells, m_cellWidths, center, halfExtents, positionLookup());
	}

	/// \brief Collect the deduplicated indices of the primitives whose position lies inside a query sphere.
	///
	/// \param center Sphere center in world coordinates.
	/// \param radius Sphere radius in world units.
	///
	/// \return The matching primitive indices, sorted ascending, each occurring once.
	[[nodiscard]] auto querySphereIndices (const glm::vec3 &center, float radius) const
		-> std::vector<std::size_t> {
		return ::querySphereIndices(m_cells, m_cellWidths, center, radius, positionLookup());
	}

	/// \brief Collect the primitives whose position lies inside an axis-aligned query box.
	///
	/// \param center Box center in world coordinates.
	/// \param halfExtents Box half-extents per axis in world units.
	///
	/// \return The matching primitives, ordered by ascending index.
	[[nodiscard]] auto queryCuboid (const glm::vec3 &center, const glm::vec3 &halfExtents) const
		-> std::vector<P>
	{
		const auto indices = queryCuboidIndices(center, halfExtents);
		std::vector<P> matches;
		matches.reserve(indices.size());
		for (const auto index : indices)
			matches.push_back(primitive(index));
		return matches;
	}

	/// \brief Collect the primitives whose position lies inside a query sphere.
	///
	/// \param center Sphere center in world coordinates.
	/// \param radius Sphere radius in world units.
	///
	/// \return The matching primitives, ordered by ascending index.
	[[nodiscard]] auto querySphere (const glm::vec3 &center, float radius) const -> std::vector<P>
	{
		const auto indices = querySphereIndices(center, radius);
		std::vector<P> matches;
		matches.reserve(indices.size());
		for (const auto index : indices)
			matches.push_back(primitive(index));
		return matches;
	}


private:

	////
	// Accessors

	/// A lookup of primitive positions by index, for passing to the query engines.
	[[nodiscard]] inline auto positionLookup () const -> PositionLookup {
		return [this] (std::size_t index) { return glm::vec3{primitive(index).position}; };
	}


	////
	// Fields

	/// The indexed primitives.
	Container m_primitives{};

	/// The cell widths the grid was built with; strictly positive per axis.
	glm::vec3 m_cellWidths{1.f, 1.f, 1.f};

	/// The occupied cells mapped to the indices of the primitives overlapping them.
	MapType m_cells{};
};


#endif // __2ACCELDS_HASH_GRID_H__
