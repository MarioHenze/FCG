
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>

// Implemented header
#include "gamma.h"



//////
//
// Function implementations
//

auto encodeGamma (float linear, float inverseGamma) -> float {
	// Apply the power curve before clamping to the display range, as in the built-in pass.
	return std::clamp(std::pow(linear, inverseGamma), .0f, 1.f);
}
