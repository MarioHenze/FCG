
//////
//
// Includes
//

// C++ STL
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>
#include <FCG/Extras/gui.h>
#include <FCG/Render/ellipsoid_renderer.h>
#include <FCG/Render/gpu_mesh.h>
#include <FCG/Render/mesh_renderer.h>

// Local includes
#include "first_person_camera.h"
#include "hash_grid.h"
#include "primitives.h"
#include "query_cursor.h"
#include "triangle_soup.h"



//////
//
// Classes
//

/// \brief Zero-copy random-access view of a mesh's vertices as smart handles.
///
/// Indexes the mesh's original vertex array through Mesh::vertex_handle in constant time, so no vertex data is
/// duplicated. The mesh must outlive the view and must not be topologically modified while the view is in use.
class MeshVertexView : public std::ranges::view_interface<MeshVertexView>
{
public:

	////
	// Classes

	/// Random-access iterator yielding the mesh's smart vertex handles by value.
	class Iterator
	{
		////
		// Friend declarations

		/// MeshVertexView positions new iterators at its range bounds.
		friend class MeshVertexView;


	public:

		////
		// Types

		/// Random traversal is supported.
		using iterator_concept = std::random_access_iterator_tag;

		/// Random traversal is supported.
		using iterator_category = std::random_access_iterator_tag;

		/// Iteration yields smart vertex handles.
		using value_type = OpenMesh::SmartVertexHandle;

		/// Signed distance between two iterators.
		using difference_type = std::ptrdiff_t;


		////
		// Object construction

		/// A singular iterator.
		Iterator () = default;


		////
		// Accessors

		/// The smart handle at the iterator's position.
		[[nodiscard]] auto operator* () const -> OpenMesh::SmartVertexHandle {
			return OpenMesh::make_smart(mesh->vertex_handle(static_cast<unsigned int>(index)), *mesh);
		}

		/// The smart handle at an offset from the iterator's position.
		[[nodiscard]] auto operator[] (difference_type offset) const -> OpenMesh::SmartVertexHandle {
			return *(*this + offset);
		}


		////
		// Methods

		/// Advance to the next vertex.
		auto operator++ () -> Iterator& {
			++index;
			return *this;
		}

		/// Advance to the next vertex, returning the previous position.
		auto operator++ (int) -> Iterator {
			auto previous = *this;
			++index;
			return previous;
		}

		/// Retreat to the previous vertex.
		auto operator-- () -> Iterator& {
			--index;
			return *this;
		}

		/// Retreat to the previous vertex, returning the previous position.
		auto operator-- (int) -> Iterator {
			auto previous = *this;
			--index;
			return previous;
		}

		/// Advance by an offset.
		auto operator+= (difference_type offset) -> Iterator& {
			index += offset;
			return *this;
		}

		/// Retreat by an offset.
		auto operator-= (difference_type offset) -> Iterator& {
			index -= offset;
			return *this;
		}

		/// An iterator advanced by an offset.
		[[nodiscard]] friend auto operator+ (Iterator it, difference_type offset) -> Iterator {
			return it += offset;
		}

		/// An iterator advanced by an offset.
		[[nodiscard]] friend auto operator+ (difference_type offset, Iterator it) -> Iterator {
			return it += offset;
		}

		/// An iterator retreated by an offset.
		[[nodiscard]] friend auto operator- (Iterator it, difference_type offset) -> Iterator {
			return it -= offset;
		}

		/// The signed distance between two iterators.
		[[nodiscard]] friend auto operator- (const Iterator &a, const Iterator &b) -> difference_type {
			return a.index - b.index;
		}

		/// Iterators at the same position of the same mesh compare equal.
		[[nodiscard]] friend auto operator== (const Iterator&, const Iterator&) -> bool = default;

		/// Iterators order by position.
		[[nodiscard]] friend auto operator<=> (const Iterator&, const Iterator&) = default;


	private:

		////
		// Object construction

		/// An iterator over the given mesh at the given position; only MeshVertexView creates iterators.
		Iterator (const fcg::Mesh *mesh, difference_type index) noexcept : mesh(mesh), index(index) {}


		////
		// Fields

		/// The mesh supplying the vertices; never owned.
		const fcg::Mesh *mesh = nullptr;

		/// Current vertex position.
		difference_type index = 0;
	};


	////
	// Object construction

	/// An empty view.
	MeshVertexView () = default;

	/// \brief View all vertices of a mesh in vertex-handle order.
	///
	/// \param mesh The mesh to view; it must outlive the view and stay topologically unmodified.
	explicit MeshVertexView (const fcg::Mesh &mesh) noexcept : mesh(&mesh), count(mesh.n_vertices()) {}


	////
	// Accessors

	/// Iterator to the first vertex.
	[[nodiscard]] auto begin () const noexcept -> Iterator {
		return {mesh, 0};
	}

	/// Past-the-end iterator.
	[[nodiscard]] auto end () const noexcept -> Iterator {
		return {mesh, static_cast<Iterator::difference_type>(count)};
	}

	/// Number of viewed vertices.
	[[nodiscard]] auto size () const noexcept -> std::size_t {
		return count;
	}

	/// \brief Smart handle of the vertex with the given index, in constant time.
	///
	/// \param index The vertex index, below size().
	///
	/// \return The smart vertex handle.
	[[nodiscard]] auto operator[] (std::size_t index) const -> OpenMesh::SmartVertexHandle {
		return begin()[static_cast<Iterator::difference_type>(index)];
	}


private:

	////
	// Fields

	/// The viewed mesh; never owned.
	const fcg::Mesh *mesh = nullptr;

	/// Number of viewed vertices.
	std::size_t count = 0;
};

// The hash grid indexes vertex primitives straight from the mesh, from smart-handle vectors and from plain vectors
static_assert(Primitive<VertexPrimitive>);
static_assert(PrimitiveStore<MeshVertexView, VertexPrimitive>);
static_assert(PrimitiveStore<std::vector<OpenMesh::SmartVertexHandle>, VertexPrimitive>);
static_assert(PrimitiveStore<std::vector<VertexPrimitive>, VertexPrimitive>);


/// View loaded meshes, and build and query a uniform hash grid over its vertices.
class UniformHashGridApplet : public fcg::Applet
{
public:

	////
	// Interface: fcg::Applet

	/// Return the applet's display name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string value = "Uniform Hash Grid";
		return value;
	}

	/// \brief Create the procedural startup scene and fit the camera to it.
	///
	/// \param device The device receiving GPU geometry.
	/// \param player The player supplying the camera and scene refreshes.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		// Borrow the registered applet through the framework's camera registry
		const auto registered = player.findCamera(FirstPersonCamera::appletName);
		camera = registered ? dynamic_cast<FirstPersonCamera*>(*registered) : nullptr;
		if (!camera)
			throw std::runtime_error("This task requires the `FirstPersonCamera` applet as the active camera");
		player.clearColor() = {.025f, .03f, .045f, 1.f};
		queryCursor.emplace(device);
		useTestScene(player);
		if (!mesh)
			throw std::runtime_error(error);
	}

	/// \brief
	///		Called by the Framework when something happened that requires rebuilding pipelines that render to the main
	///		window.
	///
	/// We don't manage any GPU pipelines ourselves, but we do need to refresh our renderers with the new settings.
	///
	/// \param device Unused; the player supplies the device.
	/// \param player The player supplying the effective scene target.
	///
	/// \throws std::runtime_error If the required sphere pipeline cannot be created.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		// Clear old renderers (frees their resources - they are now most likely unusable anyway)
		meshRenderer.reset();
		sphereRenderer.reset();

		// Replace sphere renderer
		auto newSphereRenderer = fcg::EllipsoidRenderer::create(
			player, {.cullMode=SDL_GPU_CULLMODE_NONE}
		);
		if (!newSphereRenderer)
			throw std::runtime_error(newSphereRenderer.error().message);
		sphereRenderer = std::move(*newSphereRenderer);

		// Replace mesh renderer
		auto newMeshRenderer = fcg::MeshRenderer::create(
			player, layout(shading), {.cullMode=cullMode(cullBackFaces)}
		);
		if (newMeshRenderer) {
			meshRenderer = std::move(*newMeshRenderer);
			if (gpuMesh)
				meshRenderingError.clear();
		} else
			meshRenderingError = newMeshRenderer.error().message;

		// Replace the query cursor's renderers and re-run the active query against them
		if (queryCursor) {
			queryCursor->recreatePipelines(player);
			if (!queryCursor->getError().empty())
				reportError(queryCursor->getError());
		}
		runQuery();
	}

	/// \brief Keep geometry and the user's camera navigation when the viewport changes.
	///
	/// \param device Unused.
	/// \param oldViewportSize Unused; this applet has no viewport-dependent resources.
	/// \param player Unused; the active camera handles its projection.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	/// Scene geometry is static; the camera applet handles navigation independently.
	void update (fcg::Device&, fcg::Player&, float) override {}

	/// \brief Display file selection, rendering controls and cached point information.
	///
	/// \param device Unused; loading uses the player's device.
	/// \param player The player supplying the window, device and camera.
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		// Seed a scaled inspector near the right edge, retaining saved settings
		const auto *viewport = ImGui::GetMainViewport();
		const auto size = player.toGuiUnits(ImVec2(380.f, 560.f));
		const auto padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - size.x - padding.x,
			viewport->WorkPos.y + padding.y}, ImGuiCond_FirstUseEver);
		const bool visible = ImGui::Begin("Mesh Loader");
		if (visible)
		{
			// Select another file through the same native-dialog workflow as the image viewer
			auto filename = meshFilepath.empty() ? std::string("Procedural triangle soup") : meshFilepath.filename().string();
			const float buttonWidth = ImGui::CalcTextSize("...").x + 2.f * ImGui::GetStyle().FramePadding.x;
			ImGui::SetNextItemWidth(-buttonWidth - ImGui::GetStyle().ItemSpacing.x);
			ImGui::BeginDisabled();
			ImGui::InputText("##meshFilepath", &filename);
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("...", {buttonWidth, 0.f}))
				openMesh(player);
			if (ImGui::Button("Use test scene"))
				useTestScene(player);

			// Keep the surface optional; spheres are always visible
			ImGui::BeginDisabled(!gpuMesh || !meshRenderer);
			if (ImGui::Checkbox("Show mesh", &showMesh))
				player.requireSceneRedraw();
			if (showMesh)
			{
				// Stage surface settings until their replacement resources are complete
				int nextShading = shading;
				bool nextCull = cullBackFaces;
				ImGui::Combo("Shading", &nextShading, "File normals\0Flat\0Smooth\0");
				ImGui::Checkbox("Cull back faces", &nextCull);
				if (nextShading != shading || nextCull != cullBackFaces)
					changeRendering(player, nextShading, nextCull);
			}
			ImGui::EndDisabled();
			if (ImGui::Checkbox("Lighting", &lighting))
				player.requireSceneRedraw();
			ImGui::BeginDisabled(!mesh);
			if (ImGui::Button("Fit view to scene"))
				fitMesh(player);
			ImGui::EndDisabled();

			// Report point bounds, without expanding them by the sphere radius
			ImGui::Separator();
			if (mesh)
				displayPointInfo();
			else
				ImGui::TextUnformatted("No mesh loaded");

			// Configure the hash grid and run interactive radius queries
			if (hashGrid) {
				ImGui::Separator();
				displayGridControls(player);
			}
			if (!meshRenderingError.empty()) {
				ImGui::Separator();
				ImGui::TextWrapped("Mesh rendering unavailable: %s", meshRenderingError.c_str());
			}
			if (!error.empty()) {
				ImGui::Separator();
				ImGui::TextWrapped("%s", error.c_str());
			}
		}
		ImGui::End();
	}

	/// \brief Draw the spheres and optional mesh using the camera's viewing state.
	///
	/// \param device Unused; resources already belong to the player's device.
	/// \param state Current viewing transformations supplied by the active camera.
	/// \param pass Active scene render pass.
	/// \param command Unsubmitted command buffer containing the pass.
	/// \param player Receives redraw requests after rendering errors.
	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *command,
		fcg::Player &player
	) override
	{
		if (!spheres || !sphereRenderer)
			return;
		const auto viewport = state.viewportSize();
		if (!viewport.x || !viewport.y)
			return;
		fcg::DrawOptions sphereOptions;
		sphereOptions.lighting.enabled = lighting;
		if (auto drawn = sphereRenderer->draw(*spheres, state, command, pass, sphereOptions); !drawn) {
			if (error != drawn.error().message) {
				reportError(drawn.error().message);
				player.requireSceneRedraw();
			}
		}
		if (showMesh && gpuMesh && meshRenderer) {
			fcg::MeshDrawOptions options;
			if (!metadata.vertices.colors && !metadata.faces.colors && !metadata.corners.colors)
				options.color = {1.f / 32.f, .25f, 2.f / 3.f, 1.f};
			options.lighting.enabled = lighting;
			meshRenderer->draw(*gpuMesh, state, command, pass, options);
		}

		// Draw the query cursor last so either shape blends over the opaque scene
		if (queryCursor) {
			fcg::DrawOptions options;
			options.lighting.enabled = lighting;
			queryCursor->render(state, command, pass, options);
			if (!queryCursor->getError().empty() && error != queryCursor->getError()) {
				reportError(queryCursor->getError());
				player.requireSceneRedraw();
			}
		}
	}


private:

	////
	// Methods

	/// Add the named per-vertex radius as one quarter of the average original positive edge length.
	[[nodiscard]] static auto addPointRadii (fcg::Mesh &source)
		-> std::expected<OpenMesh::VPropHandleT<float>, std::string>
	{
		// Sanity check
		if (std::distance(source.vertices_sbegin(), source.vertices_end()) < 1)
			return std::unexpected("The selected mesh has no points.");

		// Compulte average source edge lengths as basis for selected radius
		float totalLength = 0.;
		std::size_t edgeCount = 0;
		for (auto it = source.edges_sbegin(); it != source.edges_end(); ++it)
		{
			const auto halfedge = source.halfedge_handle(*it, 0);
			const auto &a = source.point(source.from_vertex_handle(
				static_cast<OpenMesh::HalfedgeHandle>(halfedge))
			);
			const auto &b = source.point(source.to_vertex_handle(
				static_cast<OpenMesh::HalfedgeHandle>(halfedge))
			);
			const float length = std::hypot(a[0]-b[0], a[1]-b[1], a[2]-b[2]);
			if (length > 0) {
				totalLength += length;
				++edgeCount;
			}
		}
		if (!edgeCount)
			return std::unexpected("The selected mesh has no positive-length edges to determine a point radius.");

		// Set readius to 1/4th the average edge length
		const auto radius = static_cast<float>(.25f * totalLength/edgeCount);
		if (!std::isfinite(radius) || radius <= 0)
			return std::unexpected("The point radius cannot be represented as a positive finite float.");

		// Retain a regular OpenMesh property storing the point radii (so in theory we could support varying radii)
		OpenMesh::VPropHandleT<float> property;
		if (!source.get_property_handle(property, "v:radius"))
			source.add_property(property, "v:radius");
		for (auto it = source.vertices_sbegin(); it != source.vertices_end(); ++it)
			source.property(property, *it) = radius;
		return property;
	}

	/// Upload one sphere per live vertex, reading its radius from the source mesh property.
	[[nodiscard]] static auto uploadSpheres (
		fcg::Device &device, const fcg::Mesh &source, OpenMesh::VPropHandleT<float> radii,
		std::vector<glm::vec4> &baseColors
	) -> std::expected<fcg::PrimitiveAttributes, fcg::RenderError>
	{
		std::vector<glm::vec4> positions, colors;
		std::vector<glm::vec3> extents;
		positions.reserve(source.n_vertices());
		colors.reserve(source.n_vertices());
		extents.reserve(source.n_vertices());
		for (auto it = source.vertices_sbegin(); it != source.vertices_end(); ++it) {
			const auto &p = source.point(*it);
			positions.emplace_back(p[0], p[1], p[2], 1.f);
			extents.emplace_back(source.property(radii, *it));
			if (source.has_vertex_colors() && source.data(*it).hasColor) {
				const auto &color = source.color(*it);
				colors.emplace_back(color[0], color[1], color[2], color[3]);
			} else
				colors.emplace_back(1.f / 32.f, .25f, 2.f / 3.f, 1.f);
		}
		fcg::PrimitiveAttributes attributes(device);
		auto uploaded = attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
			update.set<fcg::Attribute::Position>(std::span(positions));
			update.set<fcg::Attribute::Extent>(std::span(extents));
			update.set<fcg::Attribute::Color>(std::span(colors));
		});
		if (!uploaded)
			return std::unexpected(std::move(uploaded.error()));
		baseColors = std::move(colors);
		return attributes;
	}

	/// Display cached point counts, common radius and the original mesh bounds.
	void displayPointInfo () const
	{
		if (!ImGui::BeginTable("Point information", 2, ImGuiTableFlags_SizingStretchProp))
			return;
		const auto row = [] (const char *label, const std::string &value) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", value.c_str());
		};
		const auto coordinates = [] (const glm::dvec3 &v) {
			return std::format("{:.6g}, {:.6g}, {:.6g}", v.x, v.y, v.z);
		};
		row("Points", std::format("{}", metadata.vertexCount));
		row("Point radius", std::format("{:.6g}", pointRadius));
		if (metadata.bounds) {
			row("Bounds minimum", coordinates(metadata.bounds->minimum));
			row("Bounds maximum", coordinates(metadata.bounds->maximum));
			row("Bounds extent", coordinates(metadata.bounds->maximum - metadata.bounds->minimum));
		}
		if (hashGrid) {
			const auto &info = hashGrid->info();
			row("Cell widths", std::format("{:.4g}, {:.4g}, {:.4g}",
				info.cellWidths.x, info.cellWidths.y, info.cellWidths.z));
			row("Occupied cells", std::format("{}", info.occupiedCellCount));
			row("Stored references", std::format("{}", info.primitiveReferenceCount));
			row(
				"Avg. cells/sphere", std::format("{}",
				(float)info.primitiveReferenceCount/metadata.vertexCount)
			);
		}
		ImGui::EndTable();
	}

	/// Display controls for the hash grid's cell widths and the interactive radius query.
	void displayGridControls (fcg::Player &player)
	{
		// Adjusting the cell widths rebuilds the grid in place
		auto nextWidths = hashGrid->info().cellWidths;
		bool widthsChanged = ImGui::DragFloat3(
			"Cell widths", &nextWidths.x, 1/64.f*pointRadius, minCellWidth, 0, "%.4g"
		);
		float uniformWidth = nextWidths.x;
		// Limit uniform cells to one quarter of the smallest scene extent
		const glm::dvec3 extent = metadata.bounds
			? metadata.bounds->maximum - metadata.bounds->minimum : glm::dvec3{};
		const float maxUniformWidth = glm::max(
			minCellWidth, .25f * (float)glm::min(extent.x, glm::min(extent.y, extent.z))
		);
		const float minUniformWidth = glm::min(maxUniformWidth, glm::max(minCellWidth, .5f*pointRadius));
		ImGui::BeginDisabled(minUniformWidth == maxUniformWidth);
		if (ImGui::SliderFloat("...all", &uniformWidth, minUniformWidth, maxUniformWidth, "%.4g",
			ImGuiSliderFlags_AlwaysClamp |  ImGuiSliderFlags_Logarithmic))
		{
			nextWidths = glm::vec3{uniformWidth};
			widthsChanged = true;
		}
		ImGui::EndDisabled();
		if (widthsChanged && nextWidths != hashGrid->info().cellWidths) {
			hashGrid->rebuild(nextWidths);
			runQuery();
			player.requireSceneRedraw();
		}

		// Changing the query configuration re-runs the active query
		bool queryChanged = false;
		queryChanged |= ImGui::Checkbox("Radius query", &queryEnabled);
		ImGui::BeginDisabled(!queryEnabled);
		queryChanged |= ImGui::Checkbox("Force linear scan", &forceLinearScan);
		queryChanged |= ImGui::Combo("Query shape", &queryShape, "Cuboid\0Sphere\0");
		queryChanged |= ImGui::DragFloat3(
			"Query center", &queryCenter.x, 1/64.f*queryRadius, 0.f, 0.f, "%.4g"
		);
		queryChanged |= ImGui::DragFloat(
			"Query radius", &queryRadius, 1/128.f*queryRadius, .5f*pointRadius,
			.5f*glm::max(extent.x, glm::max(extent.y, extent.z)), "%.4g"
		);
		ImGui::EndDisabled();

		// Keep cursor and selection colors editable while the query is disabled
		ImGui::PushItemWidth(-ImGui::CalcTextSize("Selection highlight").x - ImGui::GetStyle().ItemInnerSpacing.x);
		queryChanged |= ImGui::ColorEdit4("Cursor", &cursorColor.x,
			ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
		queryChanged |= ImGui::ColorEdit3("Selection highlight", &selectionColor.x, ImGuiColorEditFlags_Float);
		ImGui::PopItemWidth();
		if (queryChanged) {
			cursorColor = glm::clamp(cursorColor, 0.f, 1.f);
			selectionColor = glm::clamp(selectionColor, 0.f, 1.f);
			runQuery();
			player.requireSceneRedraw();
		}
		if (queryEnabled) {
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Matched points: %zu", queryMatches); ImGui::SameLine();
			if (ImGui::Button("Rerun query")) {
				runQuery();
				player.requireSceneRedraw();
			}
			if (lastQueryMilliseconds) {
				const char *method = forceLinearScan||queryUsedLinearFallback ? "Linear fallback" : "Grid-accelerated";
				ImGui::Text("Last query time: %.3f ms (%s)", *lastQueryMilliseconds, method);
			} else {
				ImGui::TextUnformatted("Last query time: unavailable");
			}
		}
	}

	/// Run and time the configured query, reporting student TODOs before falling back to a linear scan.
	void runQuery ()
	{
		lastQueryMilliseconds.reset();
		queryUsedLinearFallback = false;
		queryMatches = 0;

		// Nothing to highlight without spheres
		if (!spheres)
			return;

		// Recolor the matched spheres, starting from the unhighlighted base colors
		error.clear();
		auto colors = sphereBaseColors;
		const glm::vec4 highlightColor{selectionColor, 1.f};
		if (queryEnabled && hashGrid) {
			try {
				std::vector<std::size_t> indices;
				auto started = std::chrono::steady_clock::now();
				if (!forceLinearScan) {
					try {
						indices = queryShape == 0
							? hashGrid->queryCuboidIndices(queryCenter, glm::vec3{queryRadius})
							: hashGrid->querySphereIndices(queryCenter, queryRadius);
					}
					catch (const std::logic_error &exception) {
						reportError(exception.what());
						queryUsedLinearFallback = true;
					}
				}
				if (forceLinearScan || queryUsedLinearFallback) {
					// Exclude the failed attempt and its feedback from the linear query's duration
					started = std::chrono::steady_clock::now();
					indices = queryShape == 0
						? hashGrid->queryCuboidIndicesLinear(queryCenter, glm::vec3{queryRadius})
						: hashGrid->querySphereIndicesLinear(queryCenter, queryRadius);
				}
				const auto finished = std::chrono::steady_clock::now();
				lastQueryMilliseconds = std::chrono::duration<double, std::milli>(finished - started).count();

				// Highlighting and GPU updates are outside the timed query
				queryMatches = indices.size();
				for (const auto index : indices)
					if (index < colors.size())
						colors[index] = highlightColor;
			}
			catch (const std::logic_error &exception) {
				reportError(exception.what());
			}
		}

		// Upload the recolored instances
		if (auto updated = spheres->setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
			update.set<fcg::Attribute::Color>(std::span(colors));
		}); !updated)
			reportError(std::format("Cannot highlight query matches: {}", updated.error().message));

		// Track the query volume with the cursor
		if (queryCursor) {
			queryCursor->update(queryEnabled, queryShape, queryCenter, queryRadius, cursorColor);
			if (!queryCursor->getError().empty())
				reportError(queryCursor->getError());
		}
	}

	/// Select the GPU channels for file normals, flat shading or regenerated smooth normals.
	[[nodiscard]] static auto layout (int shading) -> fcg::GPUMeshLayout {
		return {.normals=shading != 1, .colors=true, .texCoords=false};
	}

	/// Select two-sided rendering or back-face culling.
	[[nodiscard]] static auto cullMode (bool enabled) -> SDL_GPUCullMode {
		return enabled ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
	}

	/// Upload source geometry without changing its properties or polygon boundaries.
	[[nodiscard]] static auto uploadMesh (fcg::Device &device, const fcg::Mesh &source, int shading)
		-> std::expected<fcg::GPUMesh, fcg::RenderError>
	{
		// Triangulate polygons and generate missing smooth normals only when requested
		fcg::MeshPreprocessingPolicy<> policy;
		policy.triangulate = fcg::triangulateEarCut;
		if (shading != 1)
			policy.vertexNormals = fcg::vertexNormalAngleWeights;
		const fcg::GPUMeshOptions options{.layout=layout(shading)};
		if (shading != 2)
			return fcg::GPUMesh::create(device, source, policy, options);

		// Regenerate every normal on a temporary copy, preserving authored attributes in the source
		auto smooth = source;
		for (auto vertex : smooth.vertices())
			smooth.data(vertex).hasNormal = false;
		for (auto face : smooth.faces())
			smooth.data(face).hasNormal = false;
		for (auto corner : smooth.halfedges())
			smooth.data(corner).hasNormal = false;
		return fcg::GPUMesh::create(device, smooth, policy, options);
	}

	/// Retain a diagnostic for the panel and forward it to the application's log.
	void reportError (std::string message) {
		error = std::move(message);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "2accelds: %s", error.c_str());
	}

	/// Replace source geometry and its cached summary only after a complete successful upload.
	void loadMesh (fcg::Device &device, const std::filesystem::path &path, fcg::Player &player)
	{
		// Load using the registry's default topology policy and retain any previous mesh on failure
		auto loaded = fcg::MeshLoader::global().load(path, {
			.topology = fcg::MeshTopologyPolicy::Repair
		});
		if (!loaded) {
			reportError(std::format("Cannot load '{}': {}", path.string(), loaded.error().message));
			return;
		}
		replaceMesh(device, std::move(*loaded), path, player);
	}

	/// Replace with the same small scene used at startup, using the ordinary upload and fit path.
	void useTestScene (fcg::Player &player) {
		replaceMesh(player.device(), makeTriangleSoup(), {}, player);
	}

	/// Stage source data, GPU resources and fitted camera state before publishing a replacement scene.
	void replaceMesh (fcg::Device &device, fcg::Mesh source, const std::filesystem::path &path, fcg::Player &player)
	{
		camera->stopNavigation();
		auto nextMesh = std::make_unique<fcg::Mesh>(std::move(source));
		auto nextRadii = addPointRadii(*nextMesh);
		if (!nextRadii) {
			reportError(nextRadii.error());
			return;
		}
		auto nextMetadata = fcg::extra::collectMeshMetadata(*nextMesh);
		const float nextRadius = nextMesh->property(
			*nextRadii, static_cast<OpenMesh::VertexHandle>(*nextMesh->vertices_sbegin())
		);
		std::vector<glm::vec4> nextBaseColors;
		auto nextSpheres = uploadSpheres(device, *nextMesh, *nextRadii, nextBaseColors);
		if (!nextSpheres) {
			reportError(std::format("Cannot prepare spheres: {}", nextSpheres.error().message));
			return;
		}

		// Index the staged vertices in a hash grid sized to one sphere diameter
		HashGrid<VertexPrimitive, MeshVertexView> nextGrid{MeshVertexView{*nextMesh}, glm::vec3{2.f * nextRadius}};

		// A surface failure must not prevent viewing valid sphere primitives
		std::optional<fcg::GPUMesh> nextSurface;
		std::string nextSurfaceError;
		if (nextMetadata.faceCount) {
			auto uploaded = uploadMesh(device, *nextMesh, shading);
			if (!uploaded)
				nextSurfaceError = uploaded.error().message;
			else if (!uploaded->triangleCount())
				nextSurfaceError = "The selected mesh has no drawable triangles.";
			else
				nextSurface = std::move(*uploaded);
		} else {
			nextSurfaceError = "The selected mesh has no drawable faces.";
		}
		if (nextSurface && sphereRenderer && !meshRenderer)
			nextSurfaceError = meshRenderingError;

		// Stage the path before fitting; fitBounds validates candidate parameters before changing the registered camera
		auto nextPath = path;
		try {
			if (!nextMetadata.bounds)
				throw std::invalid_argument("The selected mesh has no bounds");
			camera->fitBounds(nextMetadata.bounds->minimum, nextMetadata.bounds->maximum, player.viewportSize());
		}
		catch (const std::invalid_argument &exception) {
			reportError(std::format("Cannot fit to mesh: {}", exception.what()));
			return;
		}

		// Commit all prepared scene state together
		meshFilepath.swap(nextPath);
		mesh = std::move(nextMesh);
		radiusProperty = *nextRadii;
		pointRadius = nextRadius;
		metadata = std::move(nextMetadata);
		spheres = std::move(*nextSpheres);
		hashGrid = std::move(nextGrid);
		sphereBaseColors = std::move(nextBaseColors);
		queryCenter = metadata.bounds ? glm::vec3{(metadata.bounds->minimum + metadata.bounds->maximum) * .5}
			: glm::vec3{};
		queryRadius = 2.f * nextRadius;
		gpuMesh = std::move(nextSurface);
		meshRenderingError.swap(nextSurfaceError);
		error.clear();
		runQuery();
		player.requireSceneRedraw();
	}

	/// Show the registered mesh formats and load the selected file; cancellation preserves the current scene.
	void openMesh (fcg::Player &player)
	{
		camera->stopNavigation();
		// Use the native blocking picker, keeping its result on the application thread
		auto selected = fcg::extra::showOpenFileDialog({
			.parent=player.mainWindow(), .title="Open mesh",
			.filters=fcg::extra::meshFileFilters(fcg::MeshLoader::global().fileFormats())
		});
		if (!selected) {
			reportError(std::format("File dialog failed: {}", selected.error().message));
			return;
		}
		if (!selected->paths.empty() && !selected->paths.front().empty())
			loadMesh(player.device(), selected->paths.front(), player);
	}

	/// Fit the current bounds while preserving the first-person orientation.
	void fitMesh (fcg::Player &player)
	{
		if (!metadata.bounds)
			return;
		camera->stopNavigation();
		try {
			camera->fitBounds(metadata.bounds->minimum, metadata.bounds->maximum, player.viewportSize());
			error.clear();
			player.requireSceneRedraw();
		}
		catch (const std::invalid_argument &exception) {
			reportError(std::format("Cannot fit to mesh: {}", exception.what()));
		}
	}

	/// Stage GPU geometry and pipelines before committing a surface-control selection.
	void changeRendering (fcg::Player &player, int nextShading, bool nextCull)
	{
		// A shading change requires geometry with the matching normal channel
		std::optional<fcg::GPUMesh> nextMesh;
		if (mesh && nextShading != shading) {
			auto uploaded = uploadMesh(player.device(), *mesh, nextShading);
			if (!uploaded) {
				reportError(uploaded.error().message);
				return;
			}
			nextMesh = std::move(*uploaded);
		}

		// Pipeline options and geometry always change together
		auto nextRenderer = fcg::MeshRenderer::create(player, layout(nextShading), {.cullMode=cullMode(nextCull)});
		if (!nextRenderer) {
			reportError(nextRenderer.error().message);
			return;
		}
		if (nextMesh)
			gpuMesh = std::move(*nextMesh);
		meshRenderer = std::move(*nextRenderer);
		shading = nextShading;
		cullBackFaces = nextCull;
		error.clear();
		player.requireSceneRedraw();
	}


	////
	// Fields

	/// Registered camera borrowed from Player (registration order makes it outlive this viewer).
	FirstPersonCamera *camera = nullptr;

	/// The path of the successfully loaded source mesh.
	std::filesystem::path meshFilepath;

	/// Independently owned source geometry, retained for inspection and shading changes.
	std::unique_ptr<fcg::Mesh> mesh;

	/// Named "v:radius" property on the owning mesh, one radius per live vertex.
	OpenMesh::VPropHandleT<float> radiusProperty;

	/// Common sphere radius, cached for the point information panel.
	float pointRadius = 0.f;

	/// Source information collected once after loading.
	fcg::extra::MeshMetadata metadata;

	/// Sphere instances uploaded once per successfully loaded mesh.
	std::optional<fcg::PrimitiveAttributes> spheres;

	/// Uniform hash grid over the current mesh's vertices, powering the interactive radius queries.
	std::optional<HashGrid<VertexPrimitive, MeshVertexView>> hashGrid;

	/// Visual cursor tracking the active query volume in the 3D scene.
	std::optional<QueryCursor> queryCursor;

	/// Per-vertex sphere colors without query highlighting, restored whenever the query changes.
	std::vector<glm::vec4> sphereBaseColors;

	/// Whether the interactive radius query is active.
	bool queryEnabled = false;

	/// Bypass the accelerated query and scan the primitive store directly.
	bool forceLinearScan = false;

	/// Query shape: 0 selects the cuboid query, 1 the spherical query.
	int queryShape = 0;

	/// Cursor RGBA shared by both query shapes, including its opacity.
	glm::vec4 cursorColor{1, .125f, 1, 2/3.f};

	/// Selection highlight RGB; matching point spheres are always opaque.
	glm::vec3 selectionColor{1, 0, 1};

	/// Center of the query volume in world coordinates.
	glm::vec3 queryCenter{};

	/// Query radius in world units; per-axis half-extent for the cuboid, radius for the sphere.
	float queryRadius = 1;

	/// Number of primitives matched by the most recent query.
	std::size_t queryMatches = 0;

	/// Duration of the most recent successful query in milliseconds, excluding rendering and TODO reporting.
	std::optional<double> lastQueryMilliseconds;

	/// Whether the latest query used a linear scan after an accelerated query threw.
	bool queryUsedLinearFallback = false;

	/// The sphere renderer we use.
	std::optional<fcg::EllipsoidRenderer> sphereRenderer;

	/// Immutable geometry uploaded with the current vertex layout.
	std::optional<fcg::GPUMesh> gpuMesh;

	/// The mesh renderer we use.
	std::optional<fcg::MeshRenderer> meshRenderer;

	/// Whether to draw the source surface in addition to its sphere primitives.
	bool showMesh = false;

	/// Surface mode: file normals with fallback, flat, or regenerated smooth normals.
	int shading = 0;

	/// Whether to cull back-facing triangles; false enables two-sided lighting.
	bool cullBackFaces = false;

	/// Whether to apply the renderer's eye-space directional lighting.
	bool lighting = true;

	/// Optional surface failure; sphere data and rendering remain usable.
	std::string meshRenderingError;

	/// Most recent operation failure, displayed until a subsequent operation succeeds.
	std::string error;
};



//////
//
// Function implementations
//

/// Run the mesh viewer with the exercise's first-person camera calculation.
auto main () -> int {
	return fcg::run<FirstPersonCamera, UniformHashGridApplet>(
		{.mainWindowTitle="Viewing and Acceleration Data Structures"}
	);
}
