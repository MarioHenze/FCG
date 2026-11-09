
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// FCG Framework
#include <FCG/render_state.h>

// Implemented header
#include "camera.h"



//////
//
// Matrix helper functions
//

/// \brief Build a translation matrix for a given offset vector.
///
/// \param offset The translation vector.
///
/// \return The 4x4 homogeneous translation matrix.
static glm::mat4 translate (const glm::vec3 &offset) {
	return glm::translate(glm::mat4(1), offset);
}

/// \brief Build a rotation matrix for a right-handed rotation about an arbitrary axis.
///
/// \param axis Unit-length axis of rotation in world coordinates.
/// \param angle Angle in radians, positive is counter-clockwise.
///
/// \return The 4x4 homogeneous rotation matrix.
static glm::mat4 rotateAroundAxis (const glm::vec3 &axis, float angle) {
	return glm::rotate(glm::mat4(1), angle, axis);
}

/// \brief Build a rotation matrix for a right-handed rotation about the X axis.
///
/// \param angle Angle in radians, positive is counter-clockwise.
///
/// \return The 4x4 homogeneous rotation matrix.
static glm::mat4 rotateX (float angle) {
	return rotateAroundAxis({1, 0, 0}, angle);
}

/// \brief Build a rotation matrix for a right-handed rotation about the Y axis.
///
/// \param angle Angle in radians, positive is counter-clockwise.
///
/// \return The 4x4 homogeneous rotation matrix.
static glm::mat4 rotateY (float angle) {
	return rotateAroundAxis({0, 1, 0}, angle);
}

/// \brief Build a rotation matrix for a right-handed rotation about the Z axis.
///
/// \param angle Angle in radians, positive is counter-clockwise.
///
/// \return The 4x4 homogeneous rotation matrix.
static glm::mat4 rotateZ (float angle) {
	return rotateAroundAxis({0, 0, 1}, angle);
}



//////
//
// Function implementations
//

void preloadViewMatrix (fcg::RenderState &state, const glm::vec3 &position, float yaw, float pitch)
{
	//## gap: initialize modelview matrix for the first-person camera
	//  Construct the view (world-to-camera) transformation from position, yaw, and pitch (angles in radians).
	//  When this function returns, the top of state's modelview matrix stack must contain that view matrix.
	//  At zero angles the camera looks along -Z, with +Y up. Positive yaw looks right; positive pitch looks up.
	//
	//  Think about how to place and orient a camera in the world, then "undo" that transformation for the scene.
	//  Build the elementary matrices with the provided translate(offset), rotate..(angle) helpers.
	//  All rotation angles are in radians; choose the appropriate axes and signs for the camera transform.
	//
	//  The helpers return matrices. Use state.loadModelviewMatrix(matrix) to replace the stack top, and
	//  state.mulModelviewMatrix(matrix) to apply further transforms individually: each call sets top = top * matrix.
	//  Pay attention to the multiplication order: we use column vectors, so in `A * B * point`, B acts first.
	state.loadModelviewMatrix(glm::mat4(1.f));
	//## solution
	state.loadModelviewMatrix(glm::rotate(glm::mat4(1.f), -pitch, glm::vec3(1, 0, 0)));
	state.mulModelviewMatrix(glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0, 1, 0)));
	state.mulModelviewMatrix(glm::translate(glm::mat4(1.f), -position));
	//## end
}
