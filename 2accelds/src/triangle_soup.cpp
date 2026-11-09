
//////
//
// Includes
//

// C++ STL
#include <array>
#include <stdexcept>

// Implemented header
#include "triangle_soup.h"



//////
//
// Function implementations
//

auto makeTriangleSoup () -> fcg::Mesh
{
	// Setup mesh
	fcg::Mesh mesh;
	mesh.request_vertex_colors();

	// Each triangle has its own shape and plane; all offsets are centered on their triangle's centroid
	const std::array<std::array<OpenMesh::Vec3f, 3>, 8> shapes{{
		{{{-1.1f, -.5f, -.4f}, {1.f, 0.f, -.3f}, {.1f, .5f, .7f}}},
		{{{-.6f, -1.f, .5f}, {.8f, -.4f, -.7f}, {-.2f, 1.4f, .2f}}},
		{{{0.f, -.9f, -1.1f}, {.5f, .7f, .2f}, {-.5f, .2f, .9f}}},
		{{{-1.2f, .2f, .4f}, {.3f, -.8f, -.6f}, {.9f, .6f, .2f}}},
		{{{-.9f, -.5f, .8f}, {.7f, -.7f, -.4f}, {.2f, 1.2f, -.4f}}},
		{{{-.3f, -.9f, -.9f}, {.8f, 1.f, -.2f}, {-.5f, -.1f, 1.1f}}},
		{{{-1.f, -.3f, .1f}, {.6f, -.6f, .6f}, {.4f, .9f, -.7f}}},
		{{{-.4f, -1.1f, .3f}, {.9f, .3f, .5f}, {-.5f, .8f, -.8f}}}
	}};
	const std::array<OpenMesh::Vec4f, 4> colors{{
		{.12f, .45f, .9f, 1.f}, {.15f, .75f, .65f, 1.f}, {.95f, .3f, .12f, 1.f}, {.9f, .65f, .12f, 1.f}
	}};

	// Separate left/right along X, then into far/near pairs along Z, with further separation in all three axes
	std::size_t triangle = 0;
	for (int side : {-1, 1})
		for (int group : {-1, 1})
			for (int leaf : {-1, 1})
			{
				const OpenMesh::Vec3f center{7.5f * side + group + .9f * leaf, .7f * side + 2.6f * group + 1.3f * leaf,
					-8.f + .8f * side + 4.8f * group + .7f * leaf};
				const auto &offsets = shapes[triangle++];
				const auto &color = colors[2 * (side > 0) + (group > 0)];
				std::array<fcg::Mesh::VertexHandle, 3> vertices;
				for (std::size_t i = 0; i < offsets.size(); ++i) {
					vertices[i] = mesh.add_vertex(center + offsets[i]);
					mesh.set_color(vertices[i], color);
					mesh.data(vertices[i]).hasColor = true;
				}
				if (!mesh.add_face(vertices[0], vertices[1], vertices[2]).is_valid())
					throw std::runtime_error("Cannot create procedural triangle");
			}

	// Done!
	return mesh;
}
