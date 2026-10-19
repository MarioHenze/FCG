
#ifndef __0INTRO_HISTOGRAM_H__
#define __0INTRO_HISTOGRAM_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstddef>

// FCG Framework
#include <FCG/Image/image.h>



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Player;
}



//////
//
// Structs and enums
//

/// \brief
/// 	How often each display code occurs, counted per color channel.
///
/// Codes are counted in \ref binCount uniform bins that span the display range [0,1]. With 256 bins, a bin index
/// is exactly the 8-bit display code, so the histogram describes what a viewer sees rather than the light values
/// behind it: an image's linear histogram barely changes while gamma sliders move, its code histogram moves a lot.
struct Histogram
{
	/// Number of bins spanning the display range, i.e. one bin per 1/256 of the code range.
	static constexpr auto binCount = std::size_t{256};

	/// Occurrence count per bin, indexed by channel (red, green, blue) and then by bin.
	std::array<std::array<float, binCount>, 3> bins{};

	/// Number of channel samples counted, i.e. three per visible pixel.
	std::size_t samples = 0;

	/// \brief Channel samples that lie above the display range, counted before encoding.
	///
	/// Those values cannot be represented and clamp to white, so they are reported here rather than being folded
	/// into the topmost bin. Images with a standard dynamic range have none of them.
	std::size_t clipped = 0;
};



//////
//
// Classes
//

/// \brief
/// 	ImGui panel that compares an image's linear light values against its display codes.
///
/// Draw one instance per frame from <tt>fcg::Applet::gui</tt>, handing it the two distributions to compare. The
/// panel keeps its own view settings, so it is a member of the applet rather than a local variable. Values are
/// plotted as a share of each histogram's own peak, which keeps the two columns comparable without fixing a scale.
class HistogramView
{
public:

	////
	// Methods

	/// \brief
	/// 	Draws both histograms side by side into the current ImGui window.
	///
	/// Shows the channels as separate plots because their distributions differ, and reports how much of the image
	/// lies above the display range at all. Draws a placeholder instead when the given distributions are empty.
	///
	/// \param player
	/// 	The player driving the GUI.
	/// \param input
	/// 	Linear light distribution, which only changes when a new image is loaded.
	/// \param display
	/// 	Display code distribution, which follows the gamma that is currently effective.
	void draw (fcg::Player &player, const Histogram &input, const Histogram &display);


private:

	////
	// Methods

	/// \brief
	/// 	Draws one channel of one histogram.
	///
	/// \param player
	/// 	The player driving the GUI.
	/// \param histogram
	/// 	Distribution to draw.
	/// \param channel
	/// 	Channel to draw, 0 to 2 for red, green and blue.
	/// \param width
	/// 	Width of the plot in pixels.
	void plotChannel (fcg::Player &player, const Histogram &histogram, int channel, float width);


	////
	// Fields

	/// \brief Whether to plot logarithmic counts.
	///
	/// A linear histogram of an HDR image puts a huge share of all samples into the first bin, which would leave
	/// nothing to see for the rest of the distribution.
	bool logScale = true;

	/// Whether to draw the linear input next to the display codes.
	bool showInput = true;

	/// Values prepared for ImGui, which plots plain floats.
	std::array<float, Histogram::binCount> plot{};
};



//////
//
// Functions
//

/// \brief
/// 	Counts how often each linear light value occurs in an image.
///
/// Use this for the light arriving from the image, i.e. the distribution that gamma encoding is applied to.
///
/// \param image
/// 	Image to count. Its format must be interleaved 32-bit floating point RGBA, i.e. \c SDL_PIXELFORMAT_RGBA128_FLOAT
/// 	as produced by \ref fcg::Image::converted with a linear colorspace.
/// \param out
/// 	Receives the distribution; previous content is replaced.
///
/// \throws std::runtime_error
/// 	If the image cannot be accessed, e.g. because its format differs or another operation holds its pixels.
void computeHistogram (const fcg::Image &image, Histogram &out);

/// \brief
/// 	Counts the display codes that encoding an image with a given gamma would produce.
///
/// Use this for the distribution a viewer actually sees: it follows the gamma sliders without requiring a
/// gamma-corrected image to exist, and it stays correct for conversions that happen on the GPU.
///
/// \param image
/// 	Image to count; see \ref computeHistogram for the required representation.
/// \param gamma
/// 	The power segment's gamma per channel, i.e. 2.4 for the standard sRGB curve.
/// \param out
/// 	Receives the distribution; previous content is replaced.
///
/// \throws std::runtime_error
/// 	If the image cannot be accessed; see \ref computeHistogram.
void computeDisplayHistogram (const fcg::Image &image, const std::array<float, 3> &gamma, Histogram &out);


#endif // ifndef __0INTRO_HISTOGRAM_H__
