
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/constants.hpp>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/player.h>
#include <FCG/window.h>

// Local includes
#include "first_person_camera.h"
#include "camera.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Compare finite matrix elements using epsilon as both an absolute and a relative tolerance.
[[nodiscard]] auto matricesAgree (
	const glm::mat4 &A, const glm::mat4 &B, float epsilon=64*std::numeric_limits<float>::epsilon()
) -> bool
{
	constexpr unsigned numTestPoints = 9;
	constexpr glm::vec4 testPoints[numTestPoints] = {
		{128, 512, 384, 1}, {-128, 512, 384, 1}, {128, -512, 384, 1},
		{128, 512, -384, 1}, {-128, -512, 384, 1}, {128, -512, -384, 1},
		{-128, 512, -384, 1}, {-128, -512, -384, 1}, {0, 0, 0, 1}
	};
	for (unsigned i=0; i<numTestPoints; ++i)
	{
		const auto testA = A * testPoints[i];
		const auto testB = B * testPoints[i];
		const auto diff = testA - testB;
		const auto diff_mag = glm::length(diff);
		const float eps_effective = std::max(
			epsilon*std::max(glm::length(glm::vec3(testA)), glm::length(glm::vec3(testB))), epsilon
		);
		if (diff_mag > eps_effective)
			return false;
	}
	return true;
}

/// Check if view matrix \c V agrees with the reference \c V_ref using <tt>\ref matricesAgree</tt> and if not, print a
/// warning to the console.
auto viewMatricesAgree (const glm::mat4 &V, const glm::mat4 &V_ref, const char *variant) -> bool
{
	if (!matricesAgree(V, V_ref))
	{
		// save stream precision and alignment, then set for our printing
		const auto oldPrecision = std::cerr.precision();
		const auto oldFlags = std::cerr.flags();
		std::cerr.precision(5);
		std::cerr.flags(std::ios::fixed | std::ios::showpoint);
		std::cerr << "View matrix is not correct (" << variant << "):\nyours = (\n"
		          << "\t" << V[0][0] << ", " << V[1][0] << ", " << V[2][0] << ", " << V[3][0] << "\n"
		          << "\t" << V[0][1] << ", " << V[1][1] << ", " << V[2][1] << ", " << V[3][1] << "\n"
		          << "\t" << V[0][2] << ", " << V[1][2] << ", " << V[2][2] << ", " << V[3][2] << "\n"
		          << "\t" << V[0][3] << ", " << V[1][3] << ", " << V[2][3] << ", " << V[3][3] << "\n"
		          << "), reference = (\n"
		          << "\t" << V_ref[0][0] << ", " << V_ref[1][0] << ", " << V_ref[2][0] << ", " << V_ref[3][0] << "\n"
		          << "\t" << V_ref[0][1] << ", " << V_ref[1][1] << ", " << V_ref[2][1] << ", " << V_ref[3][1] << "\n"
		          << "\t" << V_ref[0][2] << ", " << V_ref[1][2] << ", " << V_ref[2][2] << ", " << V_ref[3][2] << "\n"
		          << "\t" << V_ref[0][3] << ", " << V_ref[1][3] << ", " << V_ref[2][3] << ", " << V_ref[3][3]
		          << ")"  "\n";
		// restore stream precision and alignment
		std::cerr.precision(oldPrecision);
		std::cerr.flags(oldFlags);
		return false;
	}
	return true;
}

/// Reject nonfinite camera inputs before changing state.
void requireFinite (float value) {
	if (!std::isfinite(value))
		throw std::invalid_argument("Camera input must be finite");
}

/// Reject vectors with nonfinite components.
void requireFinite (const glm::vec3 &value) {
	for (int axis = 0; axis < 3; ++axis)
		requireFinite(value[axis]);
}

/// Convert a computed parameter only when representable as a finite float.
[[nodiscard]] auto checkedFloat (double value) -> float {
	if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
		throw std::invalid_argument("Camera result exceeds the coordinate range");
	return float(value);
}

/// Convert a computed eye position without overflowing its components.
[[nodiscard]] auto checkedPosition (const glm::dvec3 &value) -> glm::vec3 {
	return {checkedFloat(value.x), checkedFloat(value.y), checkedFloat(value.z)};
}

/// Keep the generic Camera matrix and focus helpers usable in single precision as well as the student function.
void validatePose (const fcg::CameraParameters &params)
{
	const auto &pose = params.extrinsics;
	requireFinite(pose.eye);
	const auto target = pose.eye + pose.dir;
	requireFinite(target);
	requireFinite(pose.eye + params.intrinsics.f * pose.dir);
	if (target == pose.eye)
		throw std::invalid_argument("Camera eye cannot represent its viewing direction");
	const auto actualDirection = glm::normalize(glm::dvec3(target) - glm::dvec3(pose.eye));
	if (glm::length(glm::cross(actualDirection, glm::dvec3(pose.up))) < 1e-5)
		throw std::invalid_argument("Camera eye cannot represent its orientation");
}

/// Compute a finite far plane strictly beyond the given near plane, following the framework camera convention.
[[nodiscard]] auto minimumFar (float nearPlane) -> float {
	const float result = std::max(
		nearPlane + .001f, std::nextafter(nearPlane, std::numeric_limits<float>::infinity())
	);
	requireFinite(result);
	return result;
}

/// Convert a finite direction to yaw and clamped pitch, retaining yaw for vertical requests.
[[nodiscard]] auto viewAngles (const glm::dvec3 &dir, float currentYaw) -> std::pair<float, float>
{
	const double horizontal = std::hypot(dir.x, dir.z);
	if (horizontal == 0. && dir.y == 0.)
		throw std::invalid_argument("Camera direction must be nonzero");
	const float yaw = horizontal > 0. ? float(std::atan2(dir.x, -dir.z)) : currentYaw;
	const float pitch = std::clamp(float(std::atan2(dir.y, horizontal)), -glm::radians(89.f), glm::radians(89.f));
	return {yaw, pitch};
}

/// Derive a level, orthonormal camera basis from the canonical yaw and pitch angles.
void setAxes (fcg::CameraParameters &params, float yaw, float pitch) {
	const glm::vec3 right{std::cos(yaw), 0.f, std::sin(yaw)};
	params.extrinsics.dir = {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
	params.extrinsics.up = glm::cross(right, params.extrinsics.dir);
}

// Anonymous namespace end
}



//////
//
// Class implementations
//

////
// FirstPersonCamera

auto FirstPersonCamera::name () const -> const std::string& {
	static const std::string value = appletName;
	return value;
}

void FirstPersonCamera::init (fcg::Device&, fcg::Player &player) {
	m_player = &player;
	requireSceneRedraw();
}

void FirstPersonCamera::onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) {
	requireSceneRedraw();
}

void FirstPersonCamera::onActivate (fcg::Player&) {
	stopNavigation();
	requireSceneRedraw();
}

void FirstPersonCamera::onDeactivate (fcg::Player&) noexcept {
	stopNavigation();
}

void FirstPersonCamera::stopNavigation () noexcept {
	acceptedKeys.fill(false);
	dragButtons = 0;
	skipMovement = true;
}

void FirstPersonCamera::requireSceneRedraw () const noexcept {
	if (m_player)
		m_player->requireSceneRedraw();
}

void FirstPersonCamera::commitParameters (const fcg::CameraParameters &next, float yaw, float pitch) {
	validatePose(next);
	m_params = next;
	m_yaw = yaw;
	m_pitch = pitch;
	requireSceneRedraw();
}

void FirstPersonCamera::setPose (const glm::vec3 &eye, double yaw, double pitch)
{
	requireFinite(eye);
	if (!std::isfinite(yaw) || !std::isfinite(pitch))
		throw std::invalid_argument("Camera input must be finite");

	// Keep an unchanged stored yaw exact, including the float representation of +/-pi at the wrap boundary.
	const float nextYaw = yaw == double(m_yaw) ? m_yaw : float(std::remainder(yaw, glm::two_pi<double>()));
	const float nextPitch = float(std::clamp(
		pitch, -double(glm::radians(89.f)), double(glm::radians(89.f))
	));
	auto next = m_params;
	next.extrinsics.eye = eye;
	setAxes(next, nextYaw, nextPitch);
	commitParameters(next, nextYaw, nextPitch);
}

void FirstPersonCamera::setFovY (const fcg::CameraParameters::Intrinsics::FovY &fovY)
{
	const auto *perspective = std::get_if<fcg::CameraParameters::Intrinsics::PerspectiveFov>(&fovY);
	if (!perspective)
		throw std::invalid_argument("FirstPersonCamera requires perspective projection");

	requireFinite(perspective->angle);
	auto next = m_params;
	next.intrinsics.fovY = fcg::CameraParameters::Intrinsics::PerspectiveFov{std::clamp(perspective->angle, 1.f, 179.f)};
	commitParameters(next, m_yaw, m_pitch);
}

void FirstPersonCamera::setFocalLength (float distance) {
	requireFinite(distance);
	auto next = m_params;
	next.intrinsics.f = std::max(.001f, distance);
	commitParameters(next, m_yaw, m_pitch);
}

void FirstPersonCamera::setNearPlane (float distance) {
	requireFinite(distance);
	auto next = m_params;
	next.intrinsics.zNear = std::max(.001f, distance);
	next.intrinsics.zFar = std::max(next.intrinsics.zFar, minimumFar(next.intrinsics.zNear));
	commitParameters(next, m_yaw, m_pitch);
}

void FirstPersonCamera::setFarPlane (float distance) {
	requireFinite(distance);
	auto next = m_params;
	next.intrinsics.zFar = std::max(distance, minimumFar(next.intrinsics.zNear));
	commitParameters(next, m_yaw, m_pitch);
}

void FirstPersonCamera::setEye (const glm::vec3 &eye) {
	auto next = m_params;
	next.extrinsics.eye = eye;
	commitParameters(next, m_yaw, m_pitch);
}

void FirstPersonCamera::setOrientationUnchecked (const glm::vec3 &dir, const glm::vec3&)
{
	// The base interface validates the supplied axes; only the direction determines this camera's level orientation
	requireFinite(dir);
	const auto [yaw, pitch] = viewAngles(glm::dvec3(dir), m_yaw);
	auto next = m_params;
	setAxes(next, yaw, pitch);
	commitParameters(next, yaw, pitch);
}

void FirstPersonCamera::setFocalPoint (const glm::vec3 &point)
{
	requireFinite(point);
	const auto direction = glm::dvec3(point) - glm::dvec3(m_params.extrinsics.eye);
	const float distance = checkedFloat(glm::length(direction));
	if (distance < .001f)
		/// Would be numerically unstable, and the eye is close enough to the requested focus that it hardly matters
		return;

	// Update camera parameters
	const auto [yaw, pitch] = viewAngles(direction, m_yaw);
	auto next = m_params;
	next.intrinsics.f = distance;
	setAxes(next, yaw, pitch);
	commitParameters(next, yaw, pitch);
}

void FirstPersonCamera::translateToFocalPoint (const glm::vec3 &point) {
	requireFinite(point);
	setEye(checkedPosition(glm::dvec3(point) - double(m_params.intrinsics.f) * glm::dvec3(m_params.extrinsics.dir)));
}

void FirstPersonCamera::resetRotation ()
{
	auto next = m_params;
	next.extrinsics.eye = checkedPosition(
		glm::dvec3(focalPoint()) + glm::dvec3(0, 0, m_params.intrinsics.f)
	);
	setAxes(next, 0.f, 0.f);
	commitParameters(next, 0.f, 0.f);
}

void FirstPersonCamera::resetFocus () {
	setFocalPoint(glm::vec3(0));
}

auto FirstPersonCamera::right () const -> glm::vec3 {
	return {std::cos(m_yaw), 0.f, std::sin(m_yaw)};
}

void FirstPersonCamera::turn (float deltaYaw, float deltaPitch)
{
	setPose(m_params.extrinsics.eye, double(m_yaw) + deltaYaw, double(m_pitch) + deltaPitch);
}

void FirstPersonCamera::fitBounds (const glm::dvec3 &minimum, const glm::dvec3 &maximum, const glm::uvec2 &viewport)
{
	// Stage values, not a copy of the registered applet, so failures preserve its identity and complete state
	for (int axis = 0; axis < 3; ++axis)
		if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) || minimum[axis] > maximum[axis])
			throw std::invalid_argument("Mesh bounds must be finite and ordered");
	const auto center = .5 * minimum + .5 * maximum;
	const double radius = std::max(.001, 1.1 * glm::length(.5 * maximum - .5 * minimum));
	const double aspect = double(std::max(1u, viewport.x)) / double(std::max(1u, viewport.y));
	const double halfFovY = glm::radians(.5 * std::get<fcg::CameraParameters::Intrinsics::PerspectiveFov>(m_params.intrinsics.fovY).angle);
	const double halfFovX = std::atan(std::tan(halfFovY) * aspect);
	const double distance = radius / std::sin(std::min(halfFovX, halfFovY));
	auto next = m_params;
	next.extrinsics.eye = checkedPosition(center - distance * glm::dvec3(m_params.extrinsics.dir));
	next.intrinsics.f = checkedFloat(distance);
	next.intrinsics.zNear = float(std::clamp(.01 * radius, .001, 1.));
	next.intrinsics.zFar = checkedFloat(distance + 3. * radius);
	const float nextSpeed = checkedFloat(radius);
	commitParameters(next, m_yaw, m_pitch);
	sceneSpeed = nextSpeed;
	stopNavigation();
}

void FirstPersonCamera::render (
	fcg::Device&, fcg::RenderState &state, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player&
){
	// Evaluate actual matrices at render time so any changes that happened during this frame's GUI or update hooks
	// already affect this scene pass.

	// Preload projection matrix
	const auto &viewport = state.viewportSize();
	if (viewport.x && viewport.y)
		// We can only set a projection matrix if there is a viewport. Otherwise it will stay the identity.
		state.loadProjectionMatrix(projectionMatrix(viewport));

	// Execute only the selected student calculation and use its result for the scene, even when validation fails.
	const auto &extrinsics = m_params.extrinsics;
	switch (viewMatrixVariant)
	{
		case ViewMatrixVariant::OnStackPoseFromParams:
			preloadViewMatrix(state, extrinsics.eye, m_yaw, m_pitch);
			break;
		case ViewMatrixVariant::InverseOfConcatModels:
			state.loadModelviewMatrix(viewMatrixFromInvertedConcatModels(extrinsics));
			break;
		case ViewMatrixVariant::InverseOfModel:
			state.loadModelviewMatrix(viewMatrixFromInvertedModel(extrinsics));
			break;
		case ViewMatrixVariant::ConcatOfSystems:
			state.loadModelviewMatrix(viewMatrixFromConcatOfSystems(extrinsics));
			break;
	}

	// Float lookAt rounds eye + dir; allow for that extra error when checking the student result.
	const auto reference = glm::lookAt(extrinsics.eye, extrinsics.eye + extrinsics.dir, extrinsics.up);
	viewMatricesAgree(
		state.modelviewMatrix(), reference, viewMatrixVariantLabels[static_cast<std::size_t>(viewMatrixVariant)]
	);
}

void FirstPersonCamera::onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player &player)
{
	if (event.type() == fcg::EventType::KeyDown || event.type() == fcg::EventType::KeyUp)
	{
		const auto *key = event.data<fcg::KeyEvent>();
		if (!key)
			return;
		for (std::size_t i = 0; i < movementKeys.size(); ++i)
			if (key->scancode == movementKeys[i]) {
				if (!key->repeat)
					acceptedKeys[i] = event.type() == fcg::EventType::KeyDown;
				context.markHandled();
				return;
			}
	}
	if (event.type() == fcg::EventType::MouseButtonDown || event.type() == fcg::EventType::MouseButtonUp)
	{
		const auto *button = event.data<fcg::MouseButtonEvent>();
		if (!button || (button->button != fcg::MouseButton::Left && button->button != fcg::MouseButton::Right))
			return;
		const auto mask = SDL_BUTTON_MASK(static_cast<unsigned>(button->button));
		if (event.type() == fcg::EventType::MouseButtonDown)
			dragButtons |= mask;
		else
			dragButtons &= ~mask;
		context.markHandled();
	}
	if (event.type() == fcg::EventType::MouseMotion)
	{
		const auto *motion = event.data<fcg::MouseMotionEvent>();
		if (!motion)
			return;
		dragButtons &= motion->buttons;
		if (dragButtons) {
			turn(glm::radians(sensitivity * motion->relativeX), glm::radians(-sensitivity * motion->relativeY));
			player.requireSceneRedraw();
			context.markHandled();
		}
	}
}

void FirstPersonCamera::update (fcg::Device &device, fcg::Player &player, float dt)
{
	// Reconcile releases even when GUI capture prevented their delivery to onEvent
	const auto *window = player.mainWindow();
	if (!window) {
		stopNavigation();
		return;
	}
	const auto flags = SDL_GetWindowFlags(window->handle());
	const bool active = SDL_GetKeyboardFocus() == window->handle() && !(flags & SDL_WINDOW_MINIMIZED);
	const auto &io = ImGui::GetIO();
	if (!active || io.WantCaptureKeyboard)
		acceptedKeys.fill(false);
	const bool *held = SDL_GetKeyboardState(nullptr);
	for (std::size_t i = 0; i < movementKeys.size(); ++i)
		acceptedKeys[i] = acceptedKeys[i] && held[movementKeys[i]];
	if (!active || io.WantCaptureMouse)
		dragButtons = 0;
	dragButtons &= SDL_GetMouseState(nullptr, nullptr);

	// A dialog or expensive replacement must not turn its blocking duration into one large movement step
	if (std::exchange(skipMovement, false))
		return;
	const float forward = float(acceptedKeys[0]) - float(acceptedKeys[1]);
	const float right = float(acceptedKeys[3]) - float(acceptedKeys[2]);
	const float up = float(acceptedKeys[5]) - float(acceptedKeys[4]);
	if (forward != 0.f || right != 0.f || up != 0.f)
	{
		const auto direction =
			forward*m_params.extrinsics.dir + right*this->right() + up*m_params.extrinsics.up;
		const float seconds = std::clamp(dt, 0.f, .1f);
		setEye(m_params.extrinsics.eye + glm::normalize(direction) * (sceneSpeed * speedFactor * seconds));
		// Post from movement intent even if dt is too small to change the floating-point position
		player.postRedraw();
	}
}

void FirstPersonCamera::gui (fcg::Device&, fcg::Player &player)
{
	const auto *viewport = ImGui::GetMainViewport();
	const auto padding = ImGui::GetStyle().WindowPadding;
	ImGui::SetNextWindowPos({viewport->WorkPos.x + padding.x, viewport->WorkPos.y + padding.y}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(player.toGuiUnits(ImVec2(310.f, 0.f)), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(appletName))
	{
		auto eye = m_params.extrinsics.eye;
		float yawDegrees = glm::degrees(m_yaw), pitchDegrees = glm::degrees(m_pitch);
		const bool positionChanged = ImGui::DragFloat3(
			"Position", &eye.x, .02f * sceneSpeed, 0.f, 0.f, "%.3f"
		);
		const bool yawChanged = ImGui::DragFloat(
			"Yaw", &yawDegrees, 1.f, 0.f, 0.f, "%.1f deg"
		);
		const bool pitchChanged = ImGui::DragFloat(
			"Pitch", &pitchDegrees, 1.f, -89.f, 89.f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp
		);
		if (positionChanged || yawChanged || pitchChanged) {
			try {
				// Only convert angles that were edited, so position changes cannot introduce angular rounding drift.
				setPose(
					eye, yawChanged ? glm::radians(double(yawDegrees)) : double(m_yaw),
					pitchChanged ? glm::radians(double(pitchDegrees)) : double(m_pitch)
				);
				stopNavigation();
				poseEditError.clear();
			}
			catch (const std::invalid_argument &exception) {
				poseEditError = exception.what();
			}
		}
		if (!poseEditError.empty())
			ImGui::TextWrapped("Cannot set camera pose: %s", poseEditError.c_str());
		ImGui::SeparatorText("View matrix calculation");
		for (std::size_t i=0; i<viewMatrixVariantLabels.size(); ++i) {
			const auto variant = static_cast<ViewMatrixVariant>(i);
			if (ImGui::RadioButton(viewMatrixVariantLabels[i], viewMatrixVariant == variant)) {
				viewMatrixVariant = variant;
				player.requireSceneRedraw();
			}
		}
		ImGui::Separator();
		ImGui::SliderFloat("Speed factor", &speedFactor, .01f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Sensitivity", &sensitivity, .01f, 1.f, "%.2f deg", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextWrapped("W/S: move forward/backward along the view. A/D: strafe left/right.");
		ImGui::TextWrapped("Q/E: move down/up along the current view up vector.");
		ImGui::TextWrapped(
			"Drag with either mouse button to look around. World +Y stays up; pitch is limited to +/-89 degrees."
		);
	}
	ImGui::End();
}
