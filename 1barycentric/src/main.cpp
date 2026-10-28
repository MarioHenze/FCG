
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// GLM library
#include <glm/exponential.hpp>
#include <glm/vec4.hpp>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet.h>
#include <FCG/render_state.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/Render/quad_renderer.h>

// Local includes
#include "barycentric.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Number of fragments the exercise's framebuffer holds horizontally. One fragment is one scene unit wide.
constexpr int gridWidth = 16;

/// Number of fragments the exercise's framebuffer holds vertically.
constexpr int gridHeight = 16;

/// Number of fragments to shade and draw, i.e. one per cell of the framebuffer.
constexpr std::size_t fragmentCount = std::size_t(gridWidth) * std::size_t(gridHeight);

/// \brief
/// 	Half edge length of the quad that is drawn for one fragment.
///
/// Slightly less than half a scene unit, so the gaps between neighboring quads read as the fragment grid.
constexpr float fragmentHalfSize = .46f;

/// \brief
/// 	Factor by which neighboring smooth-layer cells overlap.
///
/// Just enough that rounding cannot open a hairline seam between two cells, and small enough to stay invisible.
constexpr float smoothOverlap = 1.02f;

/// \brief
/// 	Exponent that turns the scene's linear light into a display code.
///
/// Shaders write linear light and the presentation encodes it with a gamma of about 2.2 before it reaches the
/// screen. Applying this exponent simulates that encoding, and its reciprocal undoes it.
constexpr float displayEncode = 1.f/2.2f;

/// Exponent that turns a display code back into linear light, i.e. the reciprocal of \ref displayEncode.
constexpr float displayDecode = 2.2f;

/// \brief
/// 	Converts a color as a color picker shows it, i.e. a display code, into the linear light the triangle mixes.
///
/// \param code
/// 	The color in display codes, with components in [0,1].
///
/// \return
/// 	The same color as a linear light value.
[[nodiscard]] auto decodeDisplayColor (const glm::vec3 &code) -> glm::vec3 {
	return glm::pow(code, glm::vec3(displayDecode));
}

/// Half edge length of the marker that shows which fragment the panel reports on.
constexpr float markerHalfSize = .12f;

/// \brief
/// 	Color of the framebuffer where the triangle does not reach.
///
/// Light enough that a fragment painted almost black, which is what a weight near zero looks like, still stands
/// out against it.
constexpr glm::vec4 uncoveredColor{.05f, .05f, .06f, 1.f};

/// \brief
/// 	Scene-space center of one fragment, i.e. the point at which the fragment is shaded.
///
/// The framebuffer is centered at the origin, so that the camera looks straight at it.
///
/// \param x
/// 	Fragment column, in [0, \ref gridWidth).
/// \param y
/// 	Fragment row, in [0, \ref gridHeight).
///
/// \return
/// 	The center of the fragment, offset by half a unit from its lower left corner.
auto fragmentCenter (int x, int y) -> glm::vec2 {
	return {
		float(x) + .5f - .5f*float(gridWidth), float(y) + .5f - .5f*float(gridHeight)
	};
}

/// \brief
/// 	Index of one fragment in the per-fragment arrays.
///
/// \param x
/// 	Fragment column, in [0, \ref gridWidth).
/// \param y
/// 	Fragment row, in [0, \ref gridHeight).
///
/// \return
/// 	The index of the fragment in row-major order.
auto fragmentIndex (int x, int y) -> std::size_t {
	return std::size_t(y)*std::size_t(gridWidth) + std::size_t(x);
}

/// \brief
/// 	Number of cells per axis of the smooth layer that paints the interpolation across the whole framebuffer.
///
/// Cells this small differ from their neighbors by well under one percent of the color range, so the layer reads
/// as the continuous field the interpolation describes, with the fragment grid sampled from it on top. The value
/// also bounds how coarsely the triangle's rim is stepped, so it is kept well above the fragment resolution.
constexpr int smoothResolution = 256;

/// Number of cells of the smooth layer.
constexpr std::size_t smoothCount = std::size_t(smoothResolution) * std::size_t(smoothResolution);

/// Edge length of one smooth-layer cell, in scene units.
constexpr float smoothCellSize = float(gridWidth) / float(smoothResolution);

/// The smooth layer's cells are square only if the framebuffer is.
static_assert(gridWidth == gridHeight, "the smooth layer assumes square cells");

/// \brief
/// 	Scene-space center of one cell of the smooth layer.
///
/// \param x
/// 	Cell column, in [0, \ref smoothResolution).
/// \param y
/// 	Cell row, in [0, \ref smoothResolution).
///
/// \return
/// 	The center of the cell, in the same scene units the fragments use.
auto smoothCenter (int x, int y) -> glm::vec2 {
	return {
		(float(x) + .5f)*smoothCellSize - .5f*float(gridWidth),
		(float(y) + .5f)*smoothCellSize - .5f*float(gridHeight)
	};
}

/// \brief
/// 	Index of one smooth-layer cell in its own per-cell array.
///
/// \param x
/// 	Cell column, in [0, \ref smoothResolution).
/// \param y
/// 	Cell row, in [0, \ref smoothResolution).
///
/// \return
/// 	The index of the cell in row-major order.
auto smoothIndex (int x, int y) -> std::size_t {
	return std::size_t(y)*std::size_t(smoothResolution) + std::size_t(x);
}

// Anonymous namespace end
}



//////
//
// Classes
//

/// \brief
/// 	Draws a triangle with per-vertex colors into a heavily pixelated framebuffer.
///
/// Each cell of a 16 by 16 grid is one fragment, shaded at its center and drawn as a small quad, so the color of a
/// single fragment can be read off the screen. The same shading is also evaluated at 128 by 128 points, which paints
/// the interpolation across the triangle as one continuous field instead of per fragment. Both layers can be shown
/// together, which the panel switches. The vertex colors are editable, one vertex's weight can be painted across
/// the triangle as a grey scale, and the panel also reports the weights of a chosen fragment next to the color they
/// produce.
class BarycentricApplet : public fcg::Applet
{
public:

	////
	// Types

	/// \brief
	/// 	What the applet draws of the shaded triangle.
	enum class Display
	{
		/// Only the fragment mosaic, i.e. what the rasterizer produces.
		fragments,

		/// Only the continuous field, i.e. what the interpolation describes.
		field,

		/// Both, the fragments sampling the field they are drawn on top of.
		both
	};


	////
	// Object construction/destruction

	/// The default constructor.
	BarycentricApplet() {}

	/// The destructor. Releases the graphics pipeline created for the scene render target.
	~BarycentricApplet() override {}


	////
	// Interface: fcg::Applet

	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Barycentric Interpolation";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{
		// The camera's default view height is too short, make it 20 to comfortably fit the 16 units fragment field.
		auto camera = *player.findCamera("2D Camera");
		camera->setFovY(fcg::CameraParameters::Intrinsics::OrthoExtend(20));

		// Set a background that is darker than the uncovered fragments, so the framebuffer's outline is visible.
		player.clearColor() = {0, 0, 0, 1};

		// The panel holds display codes, the triangle mixes linear light; start from one consistent state.
		for (int vertex = 0; vertex < 3; ++vertex)
			triangle.colors[vertex] = decodeDisplayColor(displayColors[vertex]);

		// The fragment positions never change, so they are set once here. The colors follow in updateShading().
		attributes.emplace(device);
		std::array<glm::vec4, fragmentCount> positions;
		for (int y = 0; y < gridHeight; ++y)
			for (int x = 0; x < gridWidth; ++x)
				positions[fragmentIndex(x, y)] = glm::vec4(fragmentCenter(x, y), .125f, 1.f);

		if (auto set = attributes->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Position>(std::span(positions));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set fragment geometry: {}", set.error().message
			));

		// The framebuffer's own area, drawn first so that it is visible even where no fragment is drawn.
		backdrop.emplace(device);
		const std::array backdropPosition{glm::vec4(.0f, .0f, -.125f, 1.f)};
		if (auto set = backdrop->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Position>(std::span(backdropPosition));
		    		update.set<fcg::Attribute::Extent>(
		    			glm::vec3(.5f*float(gridWidth), .5f*float(gridHeight), 1.f)
		    		);
		    		update.set<fcg::Attribute::Color>(uncoveredColor);
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set the framebuffer backdrop: {}", set.error().message
			));

		// The smooth layer tiles the same area far more finely, so that the interpolation reads as one continuous
		// field and the fragment grid on top of it looks like the sampling it is. Neighboring cells overlap by a
		// hair, which keeps hairline seams from appearing between them.
		smooth.emplace(device);
		std::vector<glm::vec4> smoothPositions(smoothCount);
		for (int y = 0; y < smoothResolution; ++y)
			for (int x = 0; x < smoothResolution; ++x)
				smoothPositions[smoothIndex(x, y)] = glm::vec4(smoothCenter(x, y), .0f, 1.f);

		if (auto set = smooth->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Position>(std::span(smoothPositions));
		    		const float half = .5f*smoothOverlap*smoothCellSize;
		    		update.set<fcg::Attribute::Extent>(glm::vec3(half, half, 1.f));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set the smooth layer's geometry: {}", set.error().message
			));

		// The marker is one more quad, drawn on top of the fragments.
		probeMarker.emplace(device);
		if (auto set = probeMarker->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Extent>(glm::vec3(markerHalfSize, markerHalfSize, 1.f));
		    		update.set<fcg::Attribute::Color>(glm::vec4(1.f, 1.f, 1.f, 1.f));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set the fragment marker: {}", set.error().message
			));
		updateProbeMarker();

		updateShading();
	}

	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		// Blanket-delete our current quad renderer. If recreation fails it will anyhow be unusable.
		qr.reset();

		// Init our quad renderer.
		if (auto maybeQr = fcg::QuadRenderer::create(player); maybeQr)
			qr = std::move(*maybeQr);
		else
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to create quad renderer: {}", maybeQr.error().message
			));
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		// Place the panel on the right, where it does not hide the framebuffer.
		const float initialWindowWidth = player.toGuiUnits(440.f);
		const auto &workSize = ImGui::GetMainViewport()->WorkSize;
		const auto &padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetNextWindowPos(
			{workSize.x - initialWindowWidth - padding.x, ImGui::GetMainViewport()->WorkPos.y + padding.y},
			ImGuiCond_FirstUseEver
		);
		ImGui::SetNextWindowSize({initialWindowWidth, 0.f}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Barycentric Interpolation");

		// An empty framebuffer usually means that the shading is not written yet, which is worth saying.
		if (!shadingError.empty()) {
			ImGui::TextDisabled("%s", shadingError.c_str());
			ImGui::Separator();
		}

		// Only the vertex colors are editable: the geometry stays where it is, so that the weights of a fragment
		// stay comparable between colors.
		ImGui::SeparatorText("Vertex colors");
		constexpr std::array vertexLabels{"vertex 0", "vertex 1", "vertex 2"};
		bool changed = false;
		for (int vertex = 0; vertex < 3; ++vertex)
			if (ImGui::ColorEdit3(vertexLabels[vertex], &displayColors[vertex].x)) {
				triangle.colors[vertex] = decodeDisplayColor(displayColors[vertex]);
				changed = true;
			}
		ImGui::SetItemTooltip("The colors are interpolated as linear light values, without tone mapping.");
		ImGui::TextDisabled(
			"at (%.1f, %.1f), (%.1f, %.1f) and (%.1f, %.1f)",
			triangle.positions[0].x, triangle.positions[0].y, triangle.positions[1].x, triangle.positions[1].y,
			triangle.positions[2].x, triangle.positions[2].y
		);

		ImGui::SeparatorText("Visualization");
		for (int vertex = 0; vertex < 3; ++vertex) {
			if (vertex > 0)
				ImGui::SameLine();
			ImGui::PushID(vertex);
			if (ImGui::Checkbox(vertexLabels[vertex], &showWeight[vertex])) {
				// One weight at a time: ticking a box clears the others, so the grey scale is never ambiguous.
				if (showWeight[vertex])
					for (int other = 0; other < 3; ++other)
						showWeight[other] = other == vertex;
				changed = true;
			}
			ImGui::PopID();
		}
		ImGui::SetItemTooltip(
			"Paints the weight of one vertex across the triangle: black is 0, white is 1.\n"
			"The three weights are exclusive, and only fragments inside the triangle are\n"
			"painted; the weights of a point outside it are not meaningful."
		);
		// The switch only concerns the grey scale above, and only exists while one is shown.
		ImGui::BeginDisabled(!selectedVertex());
		changed |= ImGui::Checkbox("Perceptually linear", &perceptualWeights);
		ImGui::EndDisabled();
		ImGui::SetItemTooltip(
			"On, the ramp is stored so that the display shows it as it is: equal steps of the weight\n"
			"look equally bright. Off, the grey is the weight itself as a linear light value, the same\n"
			"kind of number the interpolated colors are made of - the display brightens that, so the\n"
			"middle of the ramp washes out towards white."
		);
		constexpr const char *displayNames[]{
			"Fragment mosaic", "Smooth field", "Mosaic over smooth field"
		};
		ImGui::SetNextItemWidth(-ImGui::GetFontSize()*7.f);
		int displayChoice = int(display);
		if (ImGui::Combo("Display", &displayChoice, displayNames, IM_ARRAYSIZE(displayNames)))
			changed = true;
		display = Display(displayChoice);
		static const auto displayHint = std::format(
			"The smooth field is the same shading evaluated at {} by {} points instead of at the {} by {}\n"
			"fragment centers: the triangle as the interpolation describes it, with this exercise's\n"
			"sampling drawn on top. The gaps between fragments let the field show through.",
			smoothResolution, smoothResolution, gridWidth, gridHeight
		);
		ImGui::SetItemTooltip("%s", displayHint.c_str());

		ImGui::SeparatorText("Inspect a fragment");
		bool moved = false;
		moved |= ImGui::SliderInt("column", &probeX, 0, gridWidth - 1);
		moved |= ImGui::SliderInt("row", &probeY, 0, gridHeight - 1);
		drawProbe();

		ImGui::End();

		// Both updates run in the GUI callback, so the GPU data never lags behind what the panel shows. The scene
		// is only redrawn when asked, because a frame that only redraws the GUI reuses the previous scene.
		if (moved) {
			updateProbeMarker();
			player.requireSceneRedraw();
		}
		if (changed) {
			updateShading();
			player.requireSceneRedraw();
		}
	}

	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		// Nothing to update yet
	}

	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{
		fcg::DrawOptions options;

		// Depth testing is off, so the draw order is the layer order: backdrop, field, mosaic, marker.
		if (auto drawn = qr->draw(*backdrop, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);

		if (display != Display::fragments) {
			if (auto drawn = qr->draw(*smooth, rs, commandBuffer, renderPass, options); !drawn)
				throw std::runtime_error(drawn.error().message);
		}
		if (display != Display::field) {
			if (auto drawn = qr->draw(*attributes, rs, commandBuffer, renderPass, options); !drawn)
				throw std::runtime_error(drawn.error().message);
		}

		if (auto drawn = qr->draw(*probeMarker, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// \brief
	/// 	Computes the color at one point, in the mode the panel currently selects.
	///
	/// Depending on the panel, this is either the interpolated color or the grey scale of one vertex's weight.
	/// Either way only points inside the triangle are painted: the weights of a point outside it are not
	/// meaningful, and a clamped ramp there would bury the triangle they belong to.
	///
	/// \param center
	/// 	The point to shade, i.e. a fragment center or a cell center of the smooth layer.
	///
	/// \return
	/// 	The RGB color there, or \c std::nullopt if the point lies outside the triangle.
	///
	/// \throws std::logic_error
	/// 	If the exercise's code is not implemented yet.
	auto shadedColor (const glm::vec2 &center) -> std::optional<glm::vec3>
	{
		const auto vertex = selectedVertex();
		if (!vertex)
			return shadeFragment(center, triangle);

		const glm::vec3 weights = barycentricWeights(center, triangle);
		if (weights.x < 0.f || weights.y < 0.f || weights.z < 0.f)
			return std::nullopt;

		// Storing the weight as linear light is faithful, but the display then brightens it, which pushes the middle
		// of the ramp towards white. Storing the inverse of that encoding makes equal weight steps equally bright.
		const float weight = weights[*vertex];
		return glm::vec3(perceptualWeights ? std::pow(weight, displayDecode) : weight);
	}

	/// \brief
	/// 	The vertex whose weight the triangle shows, if any.
	///
	/// \return
	/// 	The index of the selected vertex, or \c std::nullopt when the triangle shows the interpolated color.
	[[nodiscard]] auto selectedVertex () const -> std::optional<int> {
		for (int vertex = 0; vertex < 3; ++vertex)
			if (showWeight[vertex])
				return vertex;
		return std::nullopt;
	}

	/// \brief
	/// 	Shades the fragments and the smooth layer, and hands both to the GPU.
	///
	/// Both layers are the same shading evaluated at different points: the fragment grid samples it once per
	/// fragment center, the smooth layer evaluates it densely enough to cover the triangle continuously.
	///
	/// A shading step that throws leaves the framebuffer empty instead of ending the program: the exercise's own
	/// code is under development, and the panel reports what is still missing.
	void updateShading ()
	{
		std::array<glm::vec4, fragmentCount> fragmentColors;
		std::array<glm::vec3, fragmentCount> fragmentExtents;
		std::vector<glm::vec4> smoothColors(smoothCount);
		try {
			for (int y = 0; y < gridHeight; ++y)
				for (int x = 0; x < gridWidth; ++x)
				{
					const auto index = fragmentIndex(x, y);
					const auto color = shadedColor(fragmentCenter(x, y));
					// A fragment outside the triangle is collapsed rather than painted, so that it cannot cut a
					// dark notch into the field behind it.
					fragmentColors[index] = glm::vec4(color.value_or(glm::vec3(uncoveredColor)), 1.f);
					fragmentExtents[index] = color
						? glm::vec3(fragmentHalfSize, fragmentHalfSize, 1.f)
						: glm::vec3(0.f);
				}
			for (int y = 0; y < smoothResolution; ++y)
				for (int x = 0; x < smoothResolution; ++x)
					smoothColors[smoothIndex(x, y)] = glm::vec4(
						shadedColor(smoothCenter(x, y)).value_or(glm::vec3(uncoveredColor)), 1.f
					);
			shadingError.clear();
		}
		catch (const std::exception &problem) {
			fragmentColors.fill(uncoveredColor);
			fragmentExtents.fill(glm::vec3(0.f));
			std::fill(smoothColors.begin(), smoothColors.end(), uncoveredColor);
			shadingError = problem.what();
		}

		if (auto set = attributes->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update) {
		    		update.set<fcg::Attribute::Color>(std::span(fragmentColors));
		    		update.set<fcg::Attribute::Extent>(std::span(fragmentExtents));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set fragment colors: {}", set.error().message
			));

		if (auto set = smooth->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update) {
		    		update.set<fcg::Attribute::Color>(std::span(smoothColors));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set the smooth layer's colors: {}", set.error().message
			));
	}

	/// \brief
	/// 	Moves the marker onto the fragment the panel reports on.
	void updateProbeMarker ()
	{
		const std::array position{glm::vec4(fragmentCenter(probeX, probeY), .25f, 1.f)};
		if (auto set = probeMarker->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update) {
		    		update.set<fcg::Attribute::Position>(std::span(position));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to move the fragment marker: {}", set.error().message
			));
	}

	/// \brief
	/// 	Draws the panel section that reports the weights of the inspected fragment.
	void drawProbe ()
	{
		const auto center = fragmentCenter(probeX, probeY);
		ImGui::Text("center (%.1f, %.1f)", center.x, center.y);

		// A failing implementation is already reported at the top of the panel.
		try {
			const glm::vec3 weights = barycentricWeights(center, triangle);
			ImGui::Text("w0 = %+.3f", weights.x);
			ImGui::Text("w1 = %+.3f", weights.y);
			ImGui::Text("w2 = %+.3f", weights.z);
			ImGui::Text("sum = %.3f", weights.x + weights.y + weights.z);
			ImGui::SetItemTooltip("The weights of a point sum to 1, no matter where the point lies.");

			// A color that does not match the weights above is what wrong interpolation looks like on screen,
			// so report the color the fragment is actually drawn with as well.
			if (const auto color = shadeFragment(center, triangle); color) {
				// The scene is presented as sRGB, so the swatch has to encode the linear value to match it.
				const auto displayColor = glm::pow(*color, glm::vec3(displayEncode));
				const float swatchSize = ImGui::GetTextLineHeight();
				ImGui::ColorButton(
					"##probeColor", ImVec4(displayColor.x, displayColor.y, displayColor.z, 1.f),
					ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize)
				);
				ImGui::SameLine();
				ImGui::Text("color (%.3f, %.3f, %.3f)", color->x, color->y, color->z);
			}
			else
				ImGui::TextDisabled("outside the triangle, so the fragment is not drawn");
		}
		catch (const std::exception&) {
			// Reported at the top of the panel, together with the empty framebuffer it explains.
		}
	}


	////
	// Fields

	/// The triangle to rasterize, with its three vertex colors as linear light.
	Triangle triangle{
		.positions = {glm::vec2(-6.5f, -6.f), glm::vec2(6.5f, -4.5f), glm::vec2(-1.5f, 6.5f)}
	};

	/// The vertex colors as the panel shows and edits them, i.e. as display codes; \ref triangle holds them decoded.
	std::array<glm::vec3, 3> displayColors{
		glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f)
	};

	/// The renderer for the fragment quads and the marker.
	std::optional<fcg::QuadRenderer> qr;

	/// One quad per fragment: its center as the position, its color as the interpolated one.
	std::optional<fcg::PrimitiveAttributes> attributes;

	/// One quad covering the framebuffer, so that its area is visible where nothing else is drawn.
	std::optional<fcg::PrimitiveAttributes> backdrop;

	/// A single quad that marks the fragment the panel reports on.
	std::optional<fcg::PrimitiveAttributes> probeMarker;

	/// One quad per cell of the fine grid that paints the interpolation as a continuous field.
	std::optional<fcg::PrimitiveAttributes> smooth;

	/// Per vertex, whether to paint its weight as a grey scale instead of the interpolated color.
	std::array<bool, 3> showWeight{};

	/// Whether that grey scale encodes the weight for the display instead of writing it as a linear light value.
	bool perceptualWeights = true;

	/// Which of the two layers, or both, the applet draws.
	Display display = Display::fragments;

	/// Column of the fragment whose weights the panel reports.
	int probeX = gridWidth / 2;

	/// Row of the fragment whose weights the panel reports.
	int probeY = gridHeight / 2;

	/// What the last shading step complained about, or empty if it ran.
	std::string shadingError;
};



//////
//
// Functions
//

/// Program entry point.
auto main () -> int {
	// Run with 2D camera and our barycentric interpolation applet
	return fcg::run<fcg::applet::Camera2D, BarycentricApplet>(fcg::PlayerSettings {
		.mainWindowTitle = "FCG Exercise - Barycentric Interpolation"
	});
}
