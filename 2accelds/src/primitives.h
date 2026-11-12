#ifndef __2ACCELDS_PRIMITIVES_H__
#define __2ACCELDS_PRIMITIVES_H__


//////
//
// Includes
//

// C++ STL
#include <vector>

// GLM library
#include <glm/glm.hpp>

// FCG Framework
#include <FCG/Mesh/mesh.h>



//////
//
// Forward declarations
//

struct GridCell;



//////
//
// Structs and enums
//

/// A sphere satisfying the Primitive concept; a zero radius represents a point.
struct SpherePrimitive
{
	/// World-space sphere center; must be finite.
	glm::vec3 position{};

	/// Sphere radius in world units; must be finite and nonnegative.
	float radius = 0.f;

	/// \brief Test intersection with another sphere, including touching boundaries.
	///
	/// \param center Other sphere's finite world-space center.
	/// \param radius Other sphere's finite, nonnegative radius in world units.
	///
	/// \return True when the spheres overlap or touch.
	[[nodiscard]] auto intersectsSphere (const glm::vec3 &center, float radius) const -> bool;

	/// \brief Test intersection with an axis-aligned box, including touching boundaries.
	///
	/// \param minCorner Box's finite minimum corner in world coordinates.
	/// \param maxCorner Box's finite maximum corner; must not be below \p minCorner along any axis.
	///
	/// \return True when the sphere and box overlap or touch.
	[[nodiscard]] auto intersectsBox (const glm::vec3 &minCorner, const glm::vec3 &maxCorner) const -> bool;

	/// \brief Append the grid cells overlapped by the sphere.
	///
	/// \param cellWidths Grid cell extents; must be positive per axis.
	/// \param cells Receives the overlapped cells in lexicographic Z-Y-X order. A zero radius selects the cell
	/// 	containing \c position.
	void voxelize (const glm::vec3 &cellWidths, std::vector<GridCell> &cells) const;
};

/// \brief A mesh vertex with its sphere radius, ready for indexing in a HashGrid.
///
/// Inherits sphere intersection and voxelization operations from SpherePrimitive. The center is the vertex
/// position and the radius comes from the mesh's "v:radius" property, defaulting to zero for a pure point.
struct VertexPrimitive: SpherePrimitive
{
	/// The source vertex in its mesh.
	OpenMesh::VertexHandle handle{};

	/// A default primitive at the origin with zero radius.
	VertexPrimitive () = default;

	/// \brief Read position and radius from a smart vertex handle's mesh.
	///
	/// \param vh The smart vertex handle; must be valid and attached to a fcg::Mesh.
	explicit VertexPrimitive (const OpenMesh::SmartVertexHandle &vh);

	/// \brief Read the vertex position and the "v:radius" property, defaulting the radius to zero when absent.
	///
	/// \param mesh The mesh owning the vertex.
	/// \param vh A valid vertex handle of \p mesh.
	///
	/// \throws std::invalid_argument If \p vh is not a valid handle.
	VertexPrimitive (const fcg::Mesh &mesh, OpenMesh::VertexHandle vh);
};


#endif // __2ACCELDS_PRIMITIVES_H__
