
//////
//
// Includes
//

// C++ STL
#include <array>
#include <format>
#include <optional>
#include <string>
#include <utility>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/Image/image.h>

// Implemented header
#include "image_metadata.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

auto name (fcg::PixelStorage value) -> const char*
{
	switch (value)
	{
		case fcg::PixelStorage::Interleaved: return "Interleaved";
		case fcg::PixelStorage::Packed: return "Packed";
		case fcg::PixelStorage::Indexed: return "Indexed";
		case fcg::PixelStorage::YUV: return "YUV";
		case fcg::PixelStorage::Compressed: return "Compressed";
		case fcg::PixelStorage::External: return "External";
	}
	return nullptr;
}

auto name (fcg::PixelRepresentation value) -> const char*
{
	switch (value)
	{
		case fcg::PixelRepresentation::UnsignedInteger: return "Unsigned normalized integer";
		case fcg::PixelRepresentation::Binary16: return "IEEE binary16 float";
		case fcg::PixelRepresentation::Binary32: return "IEEE binary32 float";
		case fcg::PixelRepresentation::PaletteIndex: return "Palette index";
		case fcg::PixelRepresentation::Unknown: return "Unknown";
	}
	return nullptr;
}

auto name (SDL_Colorspace value) -> const char*
{
	switch (value)
	{
		case SDL_COLORSPACE_UNKNOWN: return "Unknown";
		case SDL_COLORSPACE_SRGB: return "sRGB";
		case SDL_COLORSPACE_SRGB_LINEAR: return "Linear sRGB";
		case SDL_COLORSPACE_HDR10: return "HDR10";
		case SDL_COLORSPACE_JPEG: return "JPEG";
		case SDL_COLORSPACE_BT601_LIMITED: return "BT.601 limited";
		case SDL_COLORSPACE_BT601_FULL: return "BT.601 full";
		case SDL_COLORSPACE_BT709_LIMITED: return "BT.709 limited";
		case SDL_COLORSPACE_BT709_FULL: return "BT.709 full";
		case SDL_COLORSPACE_BT2020_LIMITED: return "BT.2020 limited";
		case SDL_COLORSPACE_BT2020_FULL: return "BT.2020 full";
		default: return nullptr;
	}
}

auto name (SDL_ColorType value) -> const char*
{
	switch (value) {
		case SDL_COLOR_TYPE_UNKNOWN: return "Unknown";
		case SDL_COLOR_TYPE_RGB: return "RGB";
		case SDL_COLOR_TYPE_YCBCR: return "YCbCr";
	}
	return nullptr;
}

auto name (SDL_ColorRange value) -> const char*
{
	switch (value) {
		case SDL_COLOR_RANGE_UNKNOWN: return "Unknown";
		case SDL_COLOR_RANGE_LIMITED: return "Limited";
		case SDL_COLOR_RANGE_FULL: return "Full";
	}
	return nullptr;
}

auto name (SDL_ColorPrimaries value) -> const char*
{
	switch (value)
	{
		case SDL_COLOR_PRIMARIES_UNKNOWN: return "Unknown";
		case SDL_COLOR_PRIMARIES_UNSPECIFIED: return "Unspecified";
		case SDL_COLOR_PRIMARIES_BT709: return "BT.709";
		case SDL_COLOR_PRIMARIES_BT470M: return "BT.470 M";
		case SDL_COLOR_PRIMARIES_BT470BG: return "BT.470 BG";
		case SDL_COLOR_PRIMARIES_BT601: return "BT.601";
		case SDL_COLOR_PRIMARIES_SMPTE240: return "SMPTE 240M";
		case SDL_COLOR_PRIMARIES_GENERIC_FILM: return "Generic film";
		case SDL_COLOR_PRIMARIES_BT2020: return "BT.2020";
		case SDL_COLOR_PRIMARIES_XYZ: return "XYZ (SMPTE ST 428-1)";
		case SDL_COLOR_PRIMARIES_SMPTE431: return "SMPTE RP 431-2";
		case SDL_COLOR_PRIMARIES_SMPTE432: return "SMPTE EG 432-1 (DCI P3)";
		case SDL_COLOR_PRIMARIES_EBU3213: return "EBU 3213";
		case SDL_COLOR_PRIMARIES_CUSTOM: return "Custom";
	}
	return nullptr;
}

auto name (SDL_TransferCharacteristics value) -> const char*
{
	switch (value)
	{
		case SDL_TRANSFER_CHARACTERISTICS_UNKNOWN: return "Unknown";
		case SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED: return "Unspecified";
		case SDL_TRANSFER_CHARACTERISTICS_BT709: return "BT.709";
		case SDL_TRANSFER_CHARACTERISTICS_GAMMA22: return "Gamma 2.2";
		case SDL_TRANSFER_CHARACTERISTICS_GAMMA28: return "Gamma 2.8";
		case SDL_TRANSFER_CHARACTERISTICS_BT601: return "BT.601";
		case SDL_TRANSFER_CHARACTERISTICS_SMPTE240: return "SMPTE 240M";
		case SDL_TRANSFER_CHARACTERISTICS_LINEAR: return "Linear";
		case SDL_TRANSFER_CHARACTERISTICS_LOG100: return "Logarithmic (100:1)";
		case SDL_TRANSFER_CHARACTERISTICS_LOG100_SQRT10: return "Logarithmic (100 sqrt(10):1)";
		case SDL_TRANSFER_CHARACTERISTICS_IEC61966: return "IEC 61966-2-4";
		case SDL_TRANSFER_CHARACTERISTICS_BT1361: return "BT.1361";
		case SDL_TRANSFER_CHARACTERISTICS_SRGB: return "sRGB";
		case SDL_TRANSFER_CHARACTERISTICS_BT2020_10BIT: return "BT.2020 (10-bit)";
		case SDL_TRANSFER_CHARACTERISTICS_BT2020_12BIT: return "BT.2020 (12-bit)";
		case SDL_TRANSFER_CHARACTERISTICS_PQ: return "PQ (SMPTE ST 2084)";
		case SDL_TRANSFER_CHARACTERISTICS_SMPTE428: return "SMPTE ST 428-1";
		case SDL_TRANSFER_CHARACTERISTICS_HLG: return "HLG";
		case SDL_TRANSFER_CHARACTERISTICS_CUSTOM: return "Custom";
	}
	return nullptr;
}

auto name (SDL_MatrixCoefficients value) -> const char*
{
	switch (value)
	{
		case SDL_MATRIX_COEFFICIENTS_IDENTITY: return "Identity";
		case SDL_MATRIX_COEFFICIENTS_BT709: return "BT.709";
		case SDL_MATRIX_COEFFICIENTS_UNSPECIFIED: return "Unspecified";
		case SDL_MATRIX_COEFFICIENTS_FCC: return "FCC";
		case SDL_MATRIX_COEFFICIENTS_BT470BG: return "BT.470 BG";
		case SDL_MATRIX_COEFFICIENTS_BT601: return "BT.601";
		case SDL_MATRIX_COEFFICIENTS_SMPTE240: return "SMPTE 240M";
		case SDL_MATRIX_COEFFICIENTS_YCGCO: return "YCgCo";
		case SDL_MATRIX_COEFFICIENTS_BT2020_NCL: return "BT.2020 non-constant luminance";
		case SDL_MATRIX_COEFFICIENTS_BT2020_CL: return "BT.2020 constant luminance";
		case SDL_MATRIX_COEFFICIENTS_SMPTE2085: return "SMPTE ST 2085";
		case SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL: return "Chroma-derived non-constant luminance";
		case SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL: return "Chroma-derived constant luminance";
		case SDL_MATRIX_COEFFICIENTS_ICTCP: return "ICtCp";
		case SDL_MATRIX_COEFFICIENTS_CUSTOM: return "Custom";
	}
	return nullptr;
}

auto name (SDL_ChromaLocation value) -> const char*
{
	switch (value) {
		case SDL_CHROMA_LOCATION_NONE: return "None";
		case SDL_CHROMA_LOCATION_LEFT: return "Left";
		case SDL_CHROMA_LOCATION_CENTER: return "Center";
		case SDL_CHROMA_LOCATION_TOPLEFT: return "Top left";
	}
	return nullptr;
}

template<class Enum>
auto enumText (Enum value) -> std::string {
	if (const auto *text = name(value))
		return text;
	return std::format("Unrecognized ({})", std::to_underlying(value));
}

auto yesNo (bool value) -> const char* {
	return value ? "Yes" : "No";
}

template<class Value>
auto optionalText (const std::optional<Value> &value, const char *absent) -> std::string {
	return value ? std::format("{}", *value) : absent;
}

auto channelText (const std::array<unsigned, 4> &values) -> std::string {
	return std::format("R: {}  G: {}  B: {}  A: {}", values[0], values[1], values[2], values[3]);
}

auto beginTable (const char *id) -> bool {
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		return false;
	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.45f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
	return true;
}

void row (const char *label, const std::string &value) {
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", label);
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", value.c_str());
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

void displayImageMetadata(const fcg::Image &image)
{
	ImGui::Text("%d × %d pixels", image.width(), image.height());
	const auto metadata = image.pixelInfo();
	if (!metadata) {
		ImGui::TextWrapped("Image metadata unavailable: %s", metadata.error().message.c_str());
		return;
	}
	const auto &info = *metadata;
	const auto &format = info.pixelFormat;
	const auto channelOrder = format.channelOrder.empty() ? "Unavailable" : format.channelOrder;
	const auto representation = enumText(format.representation);
	const auto colorspace = info.colorspace;
	const auto *colorspaceName = name(colorspace);
	const auto colorspaceDetails = std::format("{} (0x{:08X})",
		colorspaceName ? colorspaceName : "Custom or unrecognized", std::to_underlying(colorspace));
	const auto colorType = enumText(SDL_COLORSPACETYPE(colorspace));
	ImGui::PushID(&image);

	if (beginTable("summary")) {
		row("Channel order", channelOrder);
		row("Numeric representation", representation);
		row("Colorspace", colorspaceName ? colorspaceName : colorspaceDetails);
		row("Color type", colorType);
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Pixel storage") && beginTable("storage"))
	{
		row("Pixel format", std::format("{} (0x{:08X})", format.name, std::to_underlying(format.format)));
		row("Storage organization", enumText(format.storage));
		row("Numeric representation", representation);
		row("Channel order", channelOrder);
		row("Channel precision known", yesNo(format.channelPrecisionKnown));
		const bool indexed = format.storage == fcg::PixelStorage::Indexed;
		row(indexed ? "Palette channel bits" : "Channel sample bits",
			format.channelPrecisionKnown ? channelText(format.channelBits) : "Unavailable");
		row(indexed ? "Palette precision bits" : "Channel precision bits",
			format.channelPrecisionKnown ? channelText(format.channelPrecision) : "Unavailable");
		row("Bits per pixel", optionalText(format.bitsPerPixel, "Unavailable"));
		row("Bytes per pixel", optionalText(format.bytesPerPixel, "Unavailable"));
		// The library uses zero for unused bits when physical storage size is unavailable.
		row("Unused storage bits", format.bytesPerPixel ? std::format("{}", format.unusedBits) : "Unavailable");
		row("Row pitch", std::format("{} bytes", info.pitch));
		row("YUV sample bits", optionalText(format.yuvSampleBits, "Unavailable"));
		row("YUV storage bits", optionalText(format.yuvStorageBits, "Unavailable"));
		row("Native typed pixel access", yesNo(format.directAccess));
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Color encoding") && beginTable("encoding"))
	{
		row("Colorspace", colorspaceDetails);
		row("Color type", colorType);
		row("Color range", enumText(SDL_COLORSPACERANGE(colorspace)));
		row("Color primaries", enumText(SDL_COLORSPACEPRIMARIES(colorspace)));
		row("Transfer characteristics", enumText(SDL_COLORSPACETRANSFER(colorspace)));
		// Zero means identity/no chroma for a known colorspace, but conveys no such information for UNKNOWN.
		row("Matrix coefficients", colorspace == SDL_COLORSPACE_UNKNOWN ? "Unknown" : enumText(SDL_COLORSPACEMATRIX(colorspace)));
		row("Chroma location", colorspace == SDL_COLORSPACE_UNKNOWN ? "Unknown" : enumText(SDL_COLORSPACECHROMA(colorspace)));
		row("SDR white point (colorspace units)", optionalText(info.sdrWhitePoint, "Not specified"));
		row("HDR headroom (relative to SDR white)", optionalText(info.hdrHeadroom, "Not specified"));
		ImGui::EndTable();
	}

	if (ImGui::CollapsingHeader("Palette and transparency") && beginTable("transparency")) {
		row("Palette attached", yesNo(info.hasPalette));
		row("Palette entries", std::format("{}", info.paletteSize));
		row("Alpha capable", yesNo(format.hasAlpha));
		row("Color key (sample or index)", info.colorKey ? std::format("0x{:08X}", *info.colorKey) : "None");
		ImGui::EndTable();
	}

	ImGui::PopID();
}
