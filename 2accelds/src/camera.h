
#ifndef __2ACCELDS_CAMERA_H__
#define __2ACCELDS_CAMERA_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// GLM library
#include <glm/vec3.hpp>



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

/// \brief Preload the view (world-to-camera) matrix onto the current modelview stack entry.
///
/// The scene uses column vectors and right-handed coordinates. At zero yaw and pitch, the camera looks along
/// world -Z, with +X to its right and +Y up. Yaw turns about world Y, positive to the right; positive pitch looks up
/// around the camera's local X axis. There is no roll. Scene geometry stays in world coordinates.
///
/// The stack top is replaced independently of its previous contents. Stack depth, entries below the top, and the
/// projection stack are preserved. The resulting view maps the eye to the origin and forward to -Z.
///
/// \param state Receives the view matrix at the top of its modelview stack.
/// \param position The camera's position in world units.
/// \param yaw Horizontal viewing angle in radians.
/// \param pitch Vertical viewing angle in radians, restricted to [-89 degrees, +89 degrees] by the controller.
void preloadViewMatrix (fcg::RenderState &state, const glm::vec3 &position, float yaw, float pitch);


#endif // __2ACCELDS_CAMERA_H__
