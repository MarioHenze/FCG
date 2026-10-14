
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>

// Implemented header
#include "srgb.h"



//////
//
// Function implementations
//

auto encodeSRGB (float linear, float inverseGamma) -> float
{
	// Linear RGB is proportional to light intensity; sRGB uses nonlinear display codes. A single power such as
	// x^(1/2.2) only approximates the actual piecewise sRGB curve. Here, we're matching the SDR target range of the
	// built-in sRGB conversion (which we can opt-out of via the GUI): negative values become black, values above 1
	// become white.
	const float x = std::clamp(linear, .0f, 1.f);

	// Near black, sRGB uses a linear segment rather than a power with a very steep slope at zero.
	// The breakpoint 0.0031308 and scale 12.92 are fixed sRGB constants, independent of our gamma sliders.
	if (x <= .0031308f)
		return 12.92f * x;

	// Standard sRGB uses exponent 1/2.4, with scale 1.055 and offset 0.055 joining the two segments. Other gamma
	// values vary only this exponent: the resulting curve can jump at the breakpoint.
	return 1.055f * std::pow(x, inverseGamma) - .055f;
}
