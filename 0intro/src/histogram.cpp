
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <stdexcept>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/player.h>
#include <FCG/Image/image.h>

// Local includes
#include "histogram.h"
#include "gamma.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Plot colors, one per color channel.
constexpr std::array channelColors{
	ImVec4{1.f, .30f, .30f, 1.f}, ImVec4{.35f, .95f, .35f, 1.f},
	ImVec4{.40f, .60f, 1.f, 1.f}
};

/// \brief
/// 	Counts the three color channels of every visible pixel of an image into a histogram.
///
/// Values above 1 are outside the display range: they cannot be represented and are counted in
/// \ref Histogram::clipped as measured before encoding, while their encoded value lands in the topmost bin.
/// Alpha carries coverage rather than light intensity and is not counted.
///
/// \tparam Encode
/// 	Callable mapping one linear channel value to a display code, taking the value and the channel index.
/// \param image
/// 	Image to count, in the representation \ref computeHistogram documents.
/// \param out
/// 	Receives the bins and the counts; \ref Histogram::samples is expected to be set by the caller.
/// \param encode
/// 	Transform applied to each value before binning.
///
/// \return
/// 	Nothing, or the reason pixel access failed.
template<class Encode>
auto binImage (const fcg::Image &image, Histogram &out, Encode &&encode)
	-> std::expected<void, fcg::ImageError>
{
	//## stub: walk every visible pixel and count its three color channels
	//## gap:
	//  Hint: withPixels<SDL_PIXELFORMAT_RGBA128_FLOAT> borrows a scoped view whose rows you can iterate.
	//  Calling a pixel accessor once per sample would be far slower, because every call re-acquires the image.
	//## solution
	return image.withPixels<SDL_PIXELFORMAT_RGBA128_FLOAT>([&] (auto view) {
		for (auto row : view)
			for (int x = 0; x < row.size(); ++x)
			{
				const auto pixel = row.readUnchecked(x);
				const std::array values{pixel.r, pixel.g, pixel.b};
				for (int channel=0; channel<3; ++channel)
				{
					if (values[channel] > 1.f)
						++out.clipped;
					// The value 1 lands one bin past the end, so the index is clamped into the topmost bin.
					const auto code = std::clamp(encode(values[channel], channel), .0f, 1.f);
					const auto bin = std::min(
						std::size_t(code * float(Histogram::binCount)), Histogram::binCount - 1
					);
					++out.bins[channel][bin];
				}
			}
	});
	//## end
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

void computeHistogram (const fcg::Image &image, Histogram &out)
{
	out = Histogram{};
	out.samples = std::size_t(image.width()) * std::size_t(image.height()) * 3;
	if (const auto counted = binImage(image, out, [] (float value, int) { return value; }); !counted)
		throw std::runtime_error(counted.error().message);
}

void computeDisplayHistogram (const fcg::Image &image, const glm::vec3 &gamma, Histogram &out)
{
	out = Histogram{};
	out.samples = std::size_t(image.width()) * std::size_t(image.height()) * 3;
	const auto exponents = 1.f / gamma;
	if (const auto counted = binImage(image, out, [&exponents] (float value, int channel) {
		return encodeGamma(value, exponents[channel]);
	}); !counted)
		throw std::runtime_error(counted.error().message);
}


////
// HistogramView

void HistogramView::draw (fcg::Player &player, const Histogram &input, const Histogram &display)
{
	// An empty histogram means its binning did not run, which is worth saying instead of drawing flat plots.
	if (!input.samples || !display.samples) {
		ImGui::TextDisabled("Histogram unavailable - see the binning in histogram.cpp.");
		return;
	}

	ImGui::Checkbox("Logarithmic scale", &logScale);
	ImGui::SetItemTooltip("Plots log(1 + count), so the peak at black cannot hide the rest of the distribution.");
	ImGui::SameLine();
	ImGui::Checkbox("Show linear input", &showInput);

	// Two columns: the light values arriving from the image, and the codes the display shows for them.
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float width = showInput ? (ImGui::GetContentRegionAvail().x - spacing) * .5f
	                              : ImGui::GetContentRegionAvail().x;
	if (showInput) {
		ImGui::TextUnformatted("Linear input");
		ImGui::SameLine(width + spacing);
	}
	ImGui::TextUnformatted("Display codes");
	for (int channel = 0; channel < 3; ++channel) {
		if (showInput) {
			plotChannel(player, input, channel, width);
			ImGui::SameLine();
		}
		plotChannel(player, display, channel, width);
	}

	// How much of the image cannot be shown at all, which is the part tone mapping would have to deal with.
	ImGui::TextDisabled(
		"%.2f%% of the samples lie above the display range and clamp to white.",
		100.f * float(display.clipped)/float(display.samples)
	);
}

void HistogramView::plotChannel (fcg::Player &player, const Histogram &histogram, int channel, float width)
{
	// All channels share one scale within a column, so their heights stay comparable across plots.
	const auto &bins = histogram.bins[channel];
	const float peak = *std::max_element(bins.begin(), bins.end());
	const float scale = logScale ? std::log1p(peak) : peak;
	for (std::size_t bin = 0; bin < Histogram::binCount; ++bin)
		plot[bin] = scale > 0.f ? (logScale ? std::log1p(bins[bin]) : bins[bin]) / scale : .0f;

	// ImGui identifies plots by their label, so the two columns need separate IDs for the same channel.
	ImGui::PushID(&histogram);
	ImGui::PushID(channel);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, channelColors[channel]);
	ImGui::PlotHistogram(
		"##bins", plot.data(), int(plot.size()), 0, nullptr, .0f,
		1.f, ImVec2(width, player.toGuiUnits(64.f)));
	ImGui::PopStyleColor();
	ImGui::PopID();
	ImGui::PopID();
}
