
#ifndef __2ACCELDS_CAMERA_H__
#define __2ACCELDS_CAMERA_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// GLM library
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// FCG Framework
#include <FCG/viewing.h>



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class RenderState;
}



//////
//
// Functions
//

/// \brief Preload the view (world-to-camera) matrix onto the current modelview matrix stack.
///
/// The scene uses column vectors and right-handed coordinates. At zero yaw and pitch, the camera looks along
/// world -Z, with +X to its right and +Y up. Yaw turns about world Y, positive to the right; positive pitch looks up
/// around the camera's local X axis. There is no roll. Scene geometry stays in world coordinates.
///
/// The stack top is replaced independently of its previous contents. Stack depth, entries below the top, and the
/// projection stack are preserved. The resulting view maps the eye to the origin and forward to -Z.
///
/// \param state The render state, holding the matrix stacks for the current pass.
/// \param position The camera's position in world units.
/// \param yaw Horizontal viewing angle in radians.
/// \param pitch Vertical viewing angle in radians, restricted to [-89 degrees, +89 degrees] by the controller.
void preloadViewMatrix (fcg::RenderState &state, const glm::vec3 &position, float yaw, float pitch);

/// Obtain the view matrix by directly constructing the camera's model transformation (where the camera is in the
/// world) from the extrinsic parameters (eye, dir, up) and inverting that into a system transformation.
[[nodiscard]] auto viewMatrixFromInvertedModel (const fcg::CameraParameters::Extrinsics &extrinsics) -> glm::mat4;

/// Build the view by expressing the camera orientation (from extrinsic params dir and up) and translation (eye) as
/// separate model transforms (how the camera is rotated and where it is located in the world), then inverting the
/// concatenation into a system transformation.
[[nodiscard]] auto viewMatrixFromInvertedConcatModels (const fcg::CameraParameters::Extrinsics &extrinsics) -> glm::mat4;

/// Build the view by concatenating separate system transforms for orientation (how the scene rotates around the
/// camera) and translation (how the scene translates relative to the camera).
[[nodiscard]] auto viewMatrixFromConcatOfSystems (const fcg::CameraParameters::Extrinsics &extrinsics) -> glm::mat4;


#endif // __2ACCELDS_CAMERA_H__
