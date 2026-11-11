
#ifndef __2ACCELDS_FIRST_PERSON_CAMERA_H__
#define __2ACCELDS_FIRST_PERSON_CAMERA_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstdint>
#include <string>

// SDL3 library
#include <SDL3/SDL_scancode.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/viewing.h>



//////
//
// Classes
//

/// A perspective camera applet with WASD/QE movement, mouse look, fixed world Y up, and pitch limited to +/-89 degrees.
/// Register alongside scene applets with fcg::run; Player discovers it through the fcg::Camera interface.
/// Orientation setters remove roll and clamp pitch. setFocalPoint and resetFocus turn from the current eye;
/// translateToFocalPoint moves the eye while preserving orientation and focus distance.
/// params() describes the actual pose; render() fills the modelview stack top with the student calculation selected
/// in the GUI, defaulting to preloadViewMatrix, and validates that result against glm::lookAt.
class FirstPersonCamera : public fcg::Applet, public fcg::Camera
{
public:

	////
	// Constants

	/// Stable name used by Player::findCamera and the camera's GUI window.
	static constexpr const char *appletName = "First-person Camera";


	////
	// Object construction/destruction

	/// Create an origin camera looking along -Z, with a 60-degree perspective field of view.
	FirstPersonCamera() = default;

	/// Registered applets retain their identity and borrowed player connection.
	FirstPersonCamera(const FirstPersonCamera&) = delete;

	/// Registered applets cannot be assigned from another camera.
	auto operator= (const FirstPersonCamera&) -> FirstPersonCamera& = delete;

	/// Registered applets cannot be moved after the player has discovered them.
	FirstPersonCamera(FirstPersonCamera&&) = delete;

	/// Registered applets cannot be move-assigned.
	auto operator= (FirstPersonCamera&&) -> FirstPersonCamera& = delete;


	////
	// Interface: fcg::Applet

	/// Return the stable applet name.
	[[nodiscard]] auto name () const -> const std::string& override;

	/// Borrow the player, which outlives this applet, so programmatic setters can request scene refreshes.
	void init (fcg::Device &device, fcg::Player &player) override;

	/// This camera has no GPU pipelines.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

	/// Refresh the aspect-dependent projection without changing the pose.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override;

	/// Accept keyboard and mouse navigation after scene applets have had an opportunity to consume the event.
	void onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player &player) override;

	/// Edit the camera pose and display view-matrix calculation options, navigation settings and control instructions.
	void gui (fcg::Device &device, fcg::Player &player) override;

	/// Reconcile held input and renew postRedraw while keyboard movement is requested.
	void update (fcg::Device &device, fcg::Player &player, float dt) override;

	/// Install the selected student modelview and the provided projection, validating the view against glm::lookAt.
	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *command,
		fcg::Player &player
	) override;


	////
	// Interface: fcg::Camera

	/// Start an activation with fresh input and no movement interval carried over from dormancy.
	void onActivate (fcg::Player &player) override;

	/// Clear transient input before dormancy or shutdown, retaining pose and navigation settings.
	void onDeactivate (fcg::Player &player) noexcept override;

	/// Return parameters reflecting every setter and navigation change immediately.
	[[nodiscard]] auto params () const -> const fcg::CameraParameters& override {
		return m_params;
	}

	/// Accept finite perspective fields of view, clamped to [1,179] degrees; reject orthographic projection.
	void setFovY (const fcg::CameraParameters::Intrinsics::FovY &fovY) override;

	/// Set the framing focus distance, clamped to at least .001 world units, keeping the eye and orientation.
	void setFocalLength (float distance) override;

	/// Set a finite near plane of at least .001, expanding the far plane if necessary.
	void setNearPlane (float distance) override;

	/// Set a finite far plane, clamped to stay strictly beyond the near plane.
	void setFarPlane (float distance) override;

	/// Set a finite eye position, preserving orientation and focus distance; reject unrepresentable poses.
	void setEye (const glm::vec3 &eye) override;

	/// Convert prepared axes to yaw/pitch, removing roll and clamping pitch; vertical requests retain yaw.
	/// The inputs obey fcg::Camera::setOrientationUnchecked preconditions; the supplied up cannot introduce roll.
	void setOrientationUnchecked (const glm::vec3 &dir, const glm::vec3 &up) override;

	/// Turn toward the point without moving the eye, setting the focus distance to the eye-to-point distance.
	/// Preserve the requested horizontal heading, remove roll and clamp pitch to +/-89 degrees; exactly vertical
	/// requests retain yaw. When pitch is clamped, focalPoint() reports the attainable focus rather than the request,
	/// with the requested eye-to-point distance retained.
	/// \throws std::invalid_argument If the point is nonfinite, closer than .001 world units to the eye, or the
	/// resulting distance or pose is unrepresentable. Failure leaves the camera unchanged.
	void setFocalPoint (const glm::vec3 &point) override;

	/// Translate the eye to place the focus at this point, preserving heading and focus distance.
	void translateToFocalPoint (const glm::vec3 &point) override;

	/// Restore direction -Z and up +Y while retaining the focal point and focus distance.
	void resetRotation () override;

	/// Turn toward the world origin from the current eye, as in setFocalPoint, with the same pitch clamping. Does
	/// nothing if the eye is closer than .001 world units to the origin.
	void resetFocus () override;


	////
	// Methods

	/// Fit padded source bounds to both viewport axes, preserving orientation and adapting clipping and speed.
	/// Validation failure leaves the camera unchanged; success also resets transient navigation.
	void fitBounds (const glm::dvec3 &minimum, const glm::dvec3 &maximum, const glm::uvec2 &viewport);

	/// Cancel accepted input and discard the next movement interval, for example around a blocking file dialog.
	void stopNavigation () noexcept;


private:

	////
	// Types and constants

	enum class ViewMatrixVariant {
		OnStackPoseFromParams,
		InverseOfModel,
		InverseOfConcatModels,
		ConcatOfSystems
	};

	/// Labels follow the enum order and also identify the selected calculation in validation warnings.
	static constexpr std::array<const char*, 4> viewMatrixVariantLabels{
		"on-stack from [pos,yaw,pitch]",
		"invert camera model transform",
		"invert concatenated model transforms",
		"concatenate system transforms"
	};


	////
	// Methods

	/// Validate a candidate pose before publishing parameters and angles together and requesting a refresh.
	void commitParameters (const fcg::CameraParameters &next, float yaw, float pitch);

	/// Set an absolute pose with angles in radians, wrapping yaw and clamping pitch before committing atomically.
	void setPose (const glm::vec3 &eye, double yaw, double pitch);

	/// Request a refresh when the camera has been connected to a player.
	void requireSceneRedraw () const noexcept;

	/// Apply (presumed finite) mouse rotation, wrapping yaw and clamping pitch, without introducing roll.
	void turn (float deltaYaw, float deltaPitch);

	/// Compute the horizontal strafe direction from yaw.
	[[nodiscard]] auto right () const -> glm::vec3;


	////
	// Fields

	/// Authoritative optics and eye position; direction and up are derived from yaw/pitch on every orientation change.
	fcg::CameraParameters m_params{
		.intrinsics = {.fovY=fcg::CameraParameters::Intrinsics::PerspectiveFov{60.f}, .f=1.f, .zNear=.01f, .zFar=100.f},
		.extrinsics = {.eye={0, 0, 0}, .dir={0, 0, -1}, .up={0, 1, 0}}
	};

	/// Selected student calculation, retained when this camera is deactivated and reactivated.
	ViewMatrixVariant viewMatrixVariant = ViewMatrixVariant::OnStackPoseFromParams;

	/// Horizontal angle in radians, positive to the right and wrapped to [-pi,pi].
	float m_yaw = 0.f;

	/// Vertical angle in radians, positive upwards and restricted to +/-89 degrees.
	float m_pitch = 0.f;

	/// Last rejected GUI pose edit; cleared after the next successful edit.
	std::string poseEditError;

	/// Player borrowed during initialization; no setters require it before initialization.
	fcg::Player *m_player = nullptr;

	/// Physical keys in forward, backward, left, right, down, up order.
	static constexpr std::array movementKeys{
		SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_Q, SDL_SCANCODE_E
	};

	/// Scene-input presses not yet released or cancelled by GUI capture, focus loss or dormancy.
	std::array<bool, movementKeys.size()> acceptedKeys{};

	/// Accepted left/right mouse buttons, reconciled against current button state.
	std::uint32_t dragButtons = 0;

	/// Discard a movement interval after activation, fitting or a blocking operation.
	bool skipMovement = false;

	/// Base movement speed in world units per second, adapted to fitted scene bounds.
	float sceneSpeed = 1.f;

	/// User multiplier for the fitted movement speed.
	float speedFactor = 1.f;

	/// Mouse sensitivity in degrees per window-coordinate unit.
	float sensitivity = .2f;
};


#endif // __2ACCELDS_FIRST_PERSON_CAMERA_H__
