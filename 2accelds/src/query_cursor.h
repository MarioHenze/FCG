
#ifndef __2ACCELDS_QUERY_CURSOR_H__
#define __2ACCELDS_QUERY_CURSOR_H__


//////
//
// Includes
//

// C++ STL
#include <optional>
#include <string>

// SDL3 library
#include <SDL3/SDL_gpu.h>

// GLM library
#include <glm/glm.hpp>

// FCG Framework
#include <FCG/player.h>
#include <FCG/Render/box_renderer.h>
#include <FCG/Render/ellipsoid_renderer.h>



//////
//
// Classes
//

/// \brief Visual cursor displaying the active radius query volume in the 3D scene.
///
/// The cursor shows a transparent box for cuboid queries and a transparent sphere for spherical queries, tracking
/// the query center, radius and RGBA color supplied via \c update. Create the renderers in \c recreatePipelines and
/// draw the cursor after the opaque scene geometry, so either cursor shape blends over it.
class QueryCursor
{
public:

	////
	// Object construction

	/// \brief Create the cursor's single-instance attributes on the device.
	///
	/// \param device The device receiving the cursor geometry; must outlive the cursor.
	explicit QueryCursor (fcg::Device &device) : attributes(device) {}

	/// The cursor owns GPU resources tied to its device.
	QueryCursor(const QueryCursor&) = delete;

	/// The cursor owns GPU resources tied to its device.
	auto operator= (const QueryCursor&) -> QueryCursor& = delete;


	////
	// Methods

	/// \brief Create or replace both cursor renderers.
	///
	/// Call from \c Applet::recreatePipelines; the renderers target the player's main render pass.
	///
	/// \param player Supplies the device and main render target.
	void recreatePipelines (fcg::Player &player)
	{
		error.clear();

		// The filled box blends over the opaque scene without writing depth; cull rear faces to blend once
		if (auto renderer = fcg::BoxRenderer::create(player, {.depthWrite=false, .alphaBlending=true}))
			boxRenderer = std::move(*renderer);
		else {
			boxRenderer.reset();
			error = renderer.error().message;
		}

		// The transparent sphere blends over the scene without writing depth
		auto sphere = fcg::EllipsoidRenderer::create(
			player, {.depthWrite=false, .cullMode=SDL_GPU_CULLMODE_NONE, .alphaBlending=true}
		);
		if (sphere)
			sphereRenderer = std::move(*sphere);
		else {
			sphereRenderer.reset();
			error = sphere.error().message;
		}
	}

	/// \brief Upload the cursor attributes for the current query, or hide the cursor.
	///
	/// \param enabled Whether the radius query is active; when false, the cursor is hidden.
	/// \param shape Query shape: 0 selects the transparent box, 1 the transparent sphere.
	/// \param center Query center in world coordinates.
	/// \param radius Query radius in world units; per-axis half-extent for the cuboid, radius for the sphere.
	/// \param color RGBA color of the active cursor shape, including its opacity.
	void update (bool enabled, int shape, const glm::vec3 &center, float radius, const glm::vec4 &color)
	{
		error.clear();

		// A disabled query has no cursor; the attributes stay as they are
		visible = enabled;
		if (!enabled)
			return;

		// Upload the single cursor instance: identical extents serve as box half-extents and sphere radii
		this->shape = shape;
		const glm::vec4 position{center, 1.f};
		const glm::vec3 extent{radius};
		if (auto updated = attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &upload) {
			upload.set<fcg::Attribute::Position>(std::span{&position, 1});
			upload.set<fcg::Attribute::Extent>(std::span{&extent, 1});
			upload.set<fcg::Attribute::Color>(std::span{&color, 1});
		}); !updated)
			error = updated.error().message;
	}

	/// \brief Draw the cursor into the active pass.
	///
	/// Does nothing while the cursor is hidden or its renderer is missing. Draw after the opaque scene geometry so
	/// either cursor shape blends over it.
	///
	/// \param state Current viewing transformations supplied by the active camera.
	/// \param command Unsubmitted command buffer containing the pass.
	/// \param pass Active scene render pass.
	/// \param options Texture and lighting requests for the cursor.
	void render (
		fcg::RenderState &state, SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
		const fcg::DrawOptions &options={}
	)
	{
		error.clear();
		if (!visible)
			return;
		auto *renderer = shape == 0
			? static_cast<fcg::PrimitiveRenderer*>(boxRenderer ? &*boxRenderer : nullptr)
			: static_cast<fcg::PrimitiveRenderer*>(sphereRenderer ? &*sphereRenderer : nullptr);
		if (!renderer)
			return;
		if (auto drawn = renderer->draw(attributes, state, command, pass, options); !drawn)
			error = drawn.error().message;
	}


	////
	// Accessors

	/// The last error reported by recreatePipelines, update or render; empty when the last operation succeeded.
	[[nodiscard]] auto getError () const noexcept -> const std::string& {
		return error;
	}


private:

	////
	// Fields

	/// Single cursor instance uploaded on every query change.
	fcg::PrimitiveAttributes attributes;

	/// Renderer for the transparent cuboid query cursor.
	std::optional<fcg::BoxRenderer> boxRenderer;

	/// Renderer for the transparent spherical query cursor.
	std::optional<fcg::EllipsoidRenderer> sphereRenderer;

	/// Active query shape: 0 selects the transparent box, 1 the transparent sphere.
	int shape = 0;

	/// Whether the cursor is drawn at all.
	bool visible = false;

	/// Most recent cursor failure, displayed by the applet until the next operation succeeds.
	std::string error;
};


#endif // __2ACCELDS_QUERY_CURSOR_H__
