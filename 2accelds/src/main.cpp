
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
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
#include "triangle_soup.h"



//////
//
// Classes
//

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
		const auto size = player.toGuiUnits(ImVec2(380.f, 370.f));
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
		fcg::Device &device, const fcg::Mesh &source, OpenMesh::VPropHandleT<float> radii
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
		ImGui::EndTable();
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
		auto loaded = fcg::MeshLoader::global().load(path);
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
		auto nextSpheres = uploadSpheres(device, *nextMesh, *nextRadii);
		if (!nextSpheres) {
			reportError(std::format("Cannot prepare spheres: {}", nextSpheres.error().message));
			return;
		}

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
		gpuMesh = std::move(nextSurface);
		meshRenderingError.swap(nextSurfaceError);
		error.clear();
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
