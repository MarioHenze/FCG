
#ifndef __0INTRO_GAMMA_H__

#define __0INTRO_GAMMA_H__


//////
//
// Functions
//

/// \brief
/// 	Encodes one linear RGB component as a display code using a reciprocal-gamma power curve.
///
/// The power curve is applied before clamping the result to [0,1], matching the SDR output bounds of the built-in gamma
/// correction pass.
///
/// \pre \p linear is finite and nonnegative.
/// \pre \p inverseGamma is finite and strictly positive.
///
/// \param linear
/// 	Linear light value of one color component, before gamma correction and output clamping.
/// \param inverseGamma
/// 	The reciprocal of the gamma shown in the GUI, i.e. 1/2.2 for the default gamma.
///
/// \return
/// 	The encoded display code in [0,1].
[[nodiscard]] auto encodeGamma (float linear, float inverseGamma) -> float;


#endif // ifndef __0INTRO_GAMMA_H__
