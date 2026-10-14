
#ifndef __0INTRO_SRGB_H__

#define __0INTRO_SRGB_H__


//////
//
// Functions
//

/// \brief
/// 	Encodes one linear RGB component as an sRGB display code using an adjustable power-segment gamma.
///
/// The sRGB curve is piecewise: values near black stay on a linear segment, which avoids the power's infinite
/// slope at zero, and everything above the breakpoint uses a scale, offset and exponent. Values below 0 are
/// encoded as black and values above 1 as white, i.e. the SDR range of the built-in conversion.
///
/// \param linear
/// 	Linear light value of one color component; values outside [0,1] are clamped.
/// \param inverseGamma
/// 	Exponent of the power segment, i.e. the reciprocal of the gamma shown in the GUI; 1/2.4 reproduces the
/// 	standard sRGB curve.
///
/// \return
/// 	The encoded display code in [0,1].
[[nodiscard]] auto encodeSRGB (float linear, float inverseGamma) -> float;


#endif // ifndef __0INTRO_SRGB_H__
