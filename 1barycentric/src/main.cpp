
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

// GLM library
#include <glm/common.hpp>
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

/// Half edge length of the marker that shows which fragment the panel reports on.
constexpr float markerHalfSize = .12f;

/// Color of the fragments the triangle does not cover, i.e. the framebuffer's clear color.
constexpr glm::vec4 uncoveredColor{.02f, .02f, .025f, 1.f};

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

// Anonymous namespace end
}



//////
//
// Classes
//

/// \brief
/// 	Draws a triangle with per-corner colors into a heavily pixelated framebuffer.
///
/// Each cell of a 16 by 16 grid is one fragment, shaded at its center and drawn as a small quad, so the color of a
/// single fragment can be read off the screen. The corner colors are editable, and the panel also reports the
/// weights of a chosen fragment next to the color they produce.
class BarycentricApplet : public fcg::Applet
{
public:

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
		// Set a background that is darker than the uncovered fragments, so the framebuffer's outline is visible.
		player.clearColor() = {0, 0, 0, 1};

		// The fragment positions never change, so they are set once here. The colors follow in updateFragments().
		attributes.emplace(device);
		std::array<glm::vec4, fragmentCount> positions;
		for (int y = 0; y < gridHeight; ++y)
			for (int x = 0; x < gridWidth; ++x)
				positions[fragmentIndex(x, y)] = glm::vec4(fragmentCenter(x, y), .0f, 1.f);

		if (auto set = attributes->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Position>(std::span(positions));
		    		update.set<fcg::Attribute::Extent>(
		    			glm::vec3(fragmentHalfSize, fragmentHalfSize, 1.f)
		    		);
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set fragment geometry: {}", set.error().message
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

		updateFragments();
	}

	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		// Blanket-delete our current quad renderer. If recreation fails it will anyhow be unusable.
		qr.reset();

		// Init our quad renderer. The fragments do not overlap, but they all lie in the same plane, so depth
		// testing could only tell them apart by accident - and the marker has to cover one of them.
		if (auto maybeQr = fcg::QuadRenderer::create(
		    	player, {.depthTest=false, .depthWrite=false}
		    ); maybeQr)
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
		const float initialWindowWidth = player.toGuiUnits(380.f);
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

		// Only the corner colors are editable: the geometry stays where it is, so that the weights of a fragment
		// stay comparable between colors.
		ImGui::SeparatorText("Corner colors");
		bool changed = false;
		for (int corner = 0; corner < 3; ++corner) {
			ImGui::PushID(corner);
			changed |= ImGui::ColorEdit3("##color", &triangle.colors[corner].x);
			ImGui::PopID();
			ImGui::SameLine();
			const auto &position = triangle.positions[corner];
			ImGui::Text("corner %d at (%.1f, %.1f)", corner, position.x, position.y);
			ImGui::SetItemTooltip("The colors are interpolated as linear light values, without tone mapping.");
		}

		ImGui::SeparatorText("Visualization");
		changed |= ImGui::Checkbox("Show barycentric weights", &showWeights);
		ImGui::SetItemTooltip(
			"Colors every fragment by its own weights: red is w0, green is w1 and blue is w2,\n"
			"clamped to [0,1]. Weights change linearly across the triangle, which is exactly\n"
			"why the interpolated color does too."
		);

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
			updateFragments();
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
		if (auto drawn = qr->draw(*attributes, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);

		// The marker is drawn last, and depth testing is off, so it ends up on top of the fragment it marks.
		if (auto drawn = qr->draw(*probeMarker, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// \brief
	/// 	Computes the color of one fragment, in the mode the panel currently selects.
	///
	/// \param center
	/// 	The center of the fragment to shade.
	///
	/// \return
	/// 	The fragment's RGB color.
	///
	/// \throws std::logic_error
	/// 	If the color interpolation is not implemented yet.
	auto fragmentColor (const glm::vec2 &center) -> glm::vec3
	{
		if (showWeights)
			return glm::clamp(barycentricWeights(center, triangle), glm::vec3(0.f), glm::vec3(1.f));
		return shadeFragment(center, triangle).value_or(glm::vec3(uncoveredColor));
	}

	/// \brief
	/// 	Shades all fragments and hands their colors to the GPU.
	///
	/// A shading step that throws leaves the framebuffer empty instead of ending the program: the exercise's own
	/// code is under development, and the panel reports what is still missing.
	void updateFragments ()
	{
		std::array<glm::vec4, fragmentCount> colors;
		try {
			for (int y = 0; y < gridHeight; ++y)
				for (int x = 0; x < gridWidth; ++x)
					colors[fragmentIndex(x, y)] = glm::vec4(fragmentColor(fragmentCenter(x, y)), 1.f);
			shadingError.clear();
		}
		catch (const std::exception &problem) {
			colors.fill(uncoveredColor);
			shadingError = problem.what();
		}

		if (auto set = attributes->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
		    		update.set<fcg::Attribute::Color>(std::span(colors));
		    	}
		    ); !set)
			throw std::runtime_error(std::format(
				"Barycentric Interpolation: failed to set fragment colors: {}", set.error().message
			));
	}

	/// \brief
	/// 	Moves the marker onto the fragment the panel reports on.
	void updateProbeMarker ()
	{
		const std::array position{glm::vec4(fragmentCenter(probeX, probeY), .0f, 1.f)};
		if (auto set = probeMarker->setAttributes(
		    	[&] (fcg::PrimitiveAttributes::Update &update)
		    	{
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
				const auto displayColor = glm::pow(*color, glm::vec3(1.f/2.2f));
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

	/// The triangle to rasterize, with its three corner colors.
	Triangle triangle{
		.positions = {glm::vec2(-6.5f, -6.f), glm::vec2(6.5f, -4.5f), glm::vec2(-1.5f, 6.5f)},
		.colors = {glm::vec3(1.f, .1f, .1f), glm::vec3(.1f, 1.f, .2f), glm::vec3(.2f, .3f, 1.f)}
	};

	/// The renderer for the fragment quads and the marker.
	std::optional<fcg::QuadRenderer> qr;

	/// One quad per fragment: its center as the position, its color as the interpolated one.
	std::optional<fcg::PrimitiveAttributes> attributes;

	/// A single quad that marks the fragment the panel reports on.
	std::optional<fcg::PrimitiveAttributes> probeMarker;

	/// Whether to color the fragments by their weights instead of by their interpolated color.
	bool showWeights = false;

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
	// The camera's default view height is shorter than the framebuffer, which is 16 units tall.
	auto camera = std::make_unique<fcg::applet::Camera2D>();
	camera->setViewHeight(20.f);
	return fcg::run(std::move(camera), std::make_unique<BarycentricApplet>());
}
