
//////
//
// Includes
//

// C++ STL
#include <format>
#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <stdexcept>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/quaternion.hpp>

// Dear ImGui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet.h>
#include <FCG/frame.h>
#include <FCG/fullscreen.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/Image/image.h>
#include <FCG/Image/image_loader.h>
#include <FCG/Render/quad_renderer.h>
#include <FCG/Extras/gui.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>

// Local includes
#include "histogram.h"
#include "gamma.h"

// Baked assets
#include "0intro_assets.h"



//////
//
// Classes
//

/// \brief Displays High-dynamic-range images with various options for tone-mapping and gamma correction.
class HDRViewerApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// The default constructor.
	HDRViewerApplet() {}

	/// The destructor. Releases the graphics pipeline created during <code>\ref init</code>.
	~HDRViewerApplet() override {}


	////
	// Interface: fcg::Applet

	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "HDR Viewer";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{
		// Set black clear color
		player.clearColor() = {0, 0, 0, 1};

		// Init attribute storage for use with quad renderer
		attributes.emplace(/* fcg::PrimitiveAttributes::<ctor>: */device);

		// Create our texture sampler
		if (auto maybeSampler =fcg::Sampler::create(device, SDL_GPUSamplerCreateInfo {
		    	.min_filter = SDL_GPU_FILTER_LINEAR, .mag_filter = SDL_GPU_FILTER_LINEAR,
		    	.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		    	.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		    	.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		    }); maybeSampler)
			sampler = std::move(*maybeSampler);
		else
			throw std::runtime_error(maybeSampler.error().message);

		// Load initial placeholder image
		loadPlaceholder(device);

		// Select our plain gamma presenter instead of the player's default sRGB conversion.
		updatePresentation(device, player);
	}

	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		// Blanket-delete our current quad renderer. If recreation fails it will anyhow be unusable.
		qr.reset();

		// Init our quad renderer
		if (auto maybeQr
			= fcg::QuadRenderer::create(player, {.alphaBlending=true}); maybeQr)
			qr = std::move(*maybeQr);
		else
			throw std::runtime_error(std::format(
				"Image Viewer: failed to create quad renderer: {}", maybeQr.error().message
			));
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		static const float initialWindowWidth = player.toGuiUnits(496.f);
		const auto &workSize = ImGui::GetMainViewport()->WorkSize;
		const auto &padding = ImGui::GetStyle().WindowPadding;
		const auto initialPos = ImVec2(
			workSize.x-initialWindowWidth - padding.x, ImGui::GetMainViewport()->WorkPos.y + padding.y
		);

		// Start the loader widget
		ImGui::SetNextWindowPos({initialPos.x, initialPos.y}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize({initialWindowWidth, player.toGuiUnits(220.f)}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Image Loader");

		// File selection
		ImGui::BeginDisabled(); {
			auto filename = imageFilepath.filename().string();
			ImGui::InputText("##imageFilepath", &filename);
		} ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("..."))
			openImage(player);

		// Image metadata
		fcg::extra::displayImageMetadata(*image);

		// Finalize the loader widget
		ImGui::End();


		// Start the gamma widget
		static const float initialGammaWndHeight = player.toGuiUnits(448.f);
		ImGui::SetNextWindowPos(
			{initialPos.x, workSize.y - initialGammaWndHeight - padding.y}, ImGuiCond_FirstUseEver
		);
		ImGui::SetNextWindowSize({initialWindowWidth, initialGammaWndHeight}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Gamma Correction");

		// Where to apply gamma
		if (ImGui::Checkbox("Use built-in gamma correction", &useBuiltinGamma))
			updatePresentation(device, player);
		ImGui::SetItemTooltip(
			"NOTE: Since built-in performs gamma correction after texture filtering, the result\nwill slightly"
			" differ from the CPU computation, which happens before texture filtering."
		);

		// Gamma controls apply to both the CPU and built-in presentation.
		// - customized slider helper
		constexpr auto sliderGamma = [](const char *label, float &gamma) -> bool {
			float input = gamma;
			if (!ImGui::SliderFloat(label, &input, 0.1f, 5.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)
			    || !std::isfinite(input) || input == gamma)
				return false;
			gamma = input;
			return true;
		};
		// - define the gamma GUI
		const bool allChanged = sliderGamma("Gamma (all)", allGamma);
		bool gammaChanged = false;
		// Activation also reapplies the remembered shared value after individual edits.
		if (allChanged || ImGui::IsItemActivated()) {
			const auto gammas = glm::vec3(allGamma);
			if (gammaUniforms.gamma != gammas) {
				gammaUniforms.gamma = gammas;
				gammaChanged = true;
			}
		}
		constexpr std::array labels{"Gamma R", "Gamma G", "Gamma B"};
		for (int channel = 0; channel < gammaUniforms.gamma.length(); ++channel)
			if (sliderGamma(labels[channel], gammaUniforms.gamma[channel]))
				gammaChanged = true;
		ImGui::TextWrapped("Ctrl+click a gamma slider to type a value.");

		// Keep the CPU image ready for mode switches; the GPU recorder reads the current uniforms directly.
		if (gammaChanged) {
			updateGammaCorrection(device);
			updateHistograms();
		}

		// What the sliders above do to the distribution of the display codes
		ImGui::SeparatorText("Histogram");
		histogramView.draw(player, inputHistogram, displayHistogram);

		// Finalize the gamma widget
		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		// Nothing to update yet
	}

	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{
		fcg::DrawOptions options;
		const auto &displayTexture = useBuiltinGamma ? texture : correctedTexture;
		options.texture = fcg::PrimitiveTexture{displayTexture->handle(), sampler->handle()};
		if (auto drawn = qr->draw(*attributes, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// \brief Select built-in gamma correction or passthrough presentation for the CPU-corrected image.
	void updatePresentation (fcg::Device &device, fcg::Player &player) const
	{
		if (useBuiltinGamma)
		{
			auto gammaPresenter = fcg::FullscreenPass::gammaCorrection(device, player.swapchainFormat());
			if (!gammaPresenter)
				throw std::runtime_error(std::format(
					"HDRViewerApplet: failed to create gamma correction presentation pass: {}",
					gammaPresenter.error().message
				));

			// The recorder is cleared before applet destruction, so it can read our current uniforms each frame.
			player.setPresentationRecorder(
				[this, gammaPresenter = std::move(*gammaPresenter)] (const fcg::PresentationContext &context)
					-> std::expected<void, fcg::FullscreenError>
				{
					const fcg::FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
					const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&gammaUniforms, 1))};
					const fcg::FullscreenBindings bindings{.samplers = {&binding, 1}, .uniforms = uniforms};
					return gammaPresenter.record(context.cmdBuffer, context.target, bindings);
				}
			);
			return;
		}

		// Obtain a no-op presentation pass
		auto passthroughPresenter = fcg::FullscreenPass::passthrough(
			device, player.swapchainFormat()
		);
		if (!passthroughPresenter)
			throw std::runtime_error(std::format(
				"HDRViewerApplet: failed to create passthrough presentation pass: {}",
				passthroughPresenter.error().message
			));

		// The player owns the no-op presentation pass through the recorder until it is replaced or cleared.
		player.setPresentationRecorder(
			[passthroughPresenter = std::move(*passthroughPresenter)] (const fcg::PresentationContext &context)
				-> std::expected<void, fcg::FullscreenError>
			{
				const fcg::FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
				const fcg::FullscreenBindings bindings{.samplers = {&binding, 1}};
				return passthroughPresenter.record(context.cmdBuffer, context.target, bindings);
			}
		);
	}

	/// \brief Load image from given file.
	void loadImage (fcg::Device &device, const std::filesystem::path &filepath)
	{
		// Load from file
		if (auto maybeImage = fcg::ImageLoader::global().load(filepath); maybeImage) {
			image = std::move(*maybeImage);
			imageFilepath = filepath;
		}
		else {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "HDRViewerApplet: failed to load image '%s'\n%s",
				filepath.string().c_str(), maybeImage.error().message.c_str()
			);
			return;
		}

		// Finalize
		onNewImage(device);
	}

	/// \brief Load the embedded placeholder image
	void loadPlaceholder (fcg::Device &device)
	{
		// Fetch the placeholder image blob from the baked assets
		constexpr auto placeholderVirtualFilePath = "fcgexlogo.png";
		const auto entry = intro::assets::FS.find(placeholderVirtualFilePath);
		if (entry == intro::assets::FS.end())
			throw std::runtime_error(std::format(
				"Image Viewer: embedded placeholder '{}' is missing", placeholderVirtualFilePath
			));
		const auto bytes = (*entry).bytes();
		if (!bytes)
			throw std::runtime_error(std::format(
				"Image Viewer: embedded placeholder '{}' is not a file", placeholderVirtualFilePath
			));

		// Decode the placeholder image from memory
		if (auto maybeImage = fcg::ImageLoader::global().load(
		    	std::as_bytes(*bytes), "png"
		    ); maybeImage)
			image = std::move(*maybeImage);
		else
			throw std::runtime_error(std::format(
				"Image Viewer: failed to decode embedded placeholder '{}': {}", placeholderVirtualFilePath,
				maybeImage.error().message
			));
		imageFilepath = "<no image loaded>";

		// Finalize
		onNewImage(device);
	}

	/// \brief Open one file with a fresh snapshot of the singleton registry's format metadata.
	void openImage (fcg::Player &player)
	{
		// Open the file dialog
		auto selected = *fcg::extra::showOpenFileDialog(fcg::extra::FileDialogOptions {
			.parent=player.mainWindow(), .title="Open image",
			.filters=fcg::extra::imageFileFilters(fcg::ImageLoader::global().fileFormats())
		});

		// Try to load the selected image if any
		if (!selected.paths.empty() && !selected.paths.front().empty())
			loadImage(player.device(), selected.paths.front());
	}

	/// \brief Upload the unchanged original image and invalidate its correction caches.
	void onNewImage (fcg::Device &device)
	{
		// Invalidate the CPU surfaces (updateGammaCorrection() lazily re-creates them with correct parameters)
		linearImage.reset();
		correctedImage.reset();
		correctedTexture.reset();
		updateGammaCorrection(device);
		// A new image brings new light values, so its input distribution is recomputed as well
		updateHistograms(/* inputChanged: */ true);

		// Upload to texture
		if (auto maybeTex = image->upload(device); maybeTex)
			texture = std::move(*maybeTex);
		else
			throw std::runtime_error(std::format(
				"HDRViewerApplet: failed to upload original image: {}", maybeTex.error().message
			));

		// Configure the quad for displaying our image
		const std::array position{glm::vec4(0.f, 0.f, 0.f, 1.f)};
		auto updated = attributes->setAttributes(
			[&] (fcg::PrimitiveAttributes::Update &update)
			{
				update.set<fcg::Attribute::Position>(std::span(position));
				update.set<fcg::Attribute::Extent>(
					glm::vec3((float)image->width()/image->height(), 1, 1)
				);
				update.set<fcg::Attribute::Orientation>(
					glm::angleAxis(glm::radians(180.f), glm::vec3(1, 0, 0))
				);
			}
		);
		if (!updated)
			throw std::runtime_error(updated.error().message);
	}

	/// \brief
	/// 	Recomputes the histograms that the GUI panel draws.
	///
	/// The input distribution only depends on the light values of the image, so it is refreshed when a new image
	/// is loaded, while the display distribution follows the same per-channel gamma in both presentation modes.
	///
	/// A computation that fails is logged and leaves an empty histogram behind: the panel is a diagnostic, so it
	/// must not keep the viewer from running.
	///
	/// \param inputChanged
	/// 	Whether a different image was loaded since the previous call.
	void updateHistograms (bool inputChanged = false)
	{
		if (!linearImage)
			return;

		try {
			if (inputChanged)
				computeHistogram(*linearImage, inputHistogram);
			computeDisplayHistogram(*linearImage, gammaUniforms.gamma, displayHistogram);
		}
		catch (const std::exception &problem) {
			inputHistogram = Histogram{};
			displayHistogram = Histogram{};
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "HDRViewerApplet: histogram unavailable: %s\n", problem.what()
			);
		}
	}

	/// \brief Apply the reciprocal-gamma power curve per channel from the unchanged linear source and upload it.
	void updateGammaCorrection (fcg::Device &device)
	{
		// Convert once per image, preserving float precision and HDR range without tone mapping.
		if (!linearImage) {
			const fcg::PixelConversionOptions options{
				.lossPolicy = fcg::PixelLossPolicy::Allow, .colorspace = SDL_COLORSPACE_SRGB_LINEAR
			};
			if (auto converted = image->converted(
			    	SDL_PIXELFORMAT_RGBA128_FLOAT, options
			    ); converted)
				linearImage = std::move(*converted);
			else
				throw std::runtime_error(std::format(
					"HDRViewerApplet: failed to convert image to linear RGB: {}", converted.error().message
				));
		}

		// Ensure destination image. If it does not yet exist, that means we either never performed gamma correction
		// before, or it was explicitely destroyed because the source image changed.
		if (!correctedImage)
		{
			// Use float 32 bits per channel (32*4=128 bits) format for easy floating point operations
			if (auto copy = linearImage->converted(SDL_PIXELFORMAT_RGBA128_FLOAT);
			    copy)
				correctedImage = std::move(*copy);
			else
				throw std::runtime_error(std::format(
					"HDRViewerApplet: failed to create gamma correction destination: {}",
					copy.error().message
				));
		}

		// Use the same reciprocal-gamma curve as the built-in pass, here before texture filtering.
		const auto exponents = 1.f / gammaUniforms.gamma;
		auto corrected = linearImage->transformPixelsTo<SDL_PIXELFORMAT_RGBA128_FLOAT>(
			*correctedImage, [exponents](int x, int y, auto sourcePxl) {
				auto outPxl = sourcePxl.readUnchecked(x, y);
				outPxl.r = encodeGamma(outPxl.r, exponents.r);
				outPxl.g = encodeGamma(outPxl.g, exponents.g);
				outPxl.b = encodeGamma(outPxl.b, exponents.b);
				return outPxl; // Alpha represents coverage rather than light intensity, so leave it unchanged.
			}
		);
		if (!corrected)
			throw std::runtime_error(std::format(
				"HDRViewerApplet: failed to apply CPU gamma correction: {}", corrected.error().message
			));

		// Pixel access has ended. The RGB values now contain display colors, but we retain linear storage metadata
		// so upload/sampling preserves these numbers. Marking them as sRGB would decode and undo our CPU curve.
		if (auto uploaded = correctedImage->upload(device); uploaded)
			correctedTexture = std::move(*uploaded);
		else
			throw std::runtime_error(std::format(
				"HDRViewerApplet: failed to upload gamma-corrected image: {}", uploaded.error().message
			));
	}


	////
	// Fields

	/// Whether to use built-in gamma correction for visually checking the CPU correction.
	bool useBuiltinGamma = true;

	/// Parameters shared by CPU correction, display histograms and our GPU presenter; defaults to gamma 2.2.
	fcg::GammaUniforms gammaUniforms{};

	/// Last shared slider value; reapplied to all channels only when that slider is used.
	float allGamma = gammaUniforms.gamma.r;

	/// The filepath of the currently loaded image
	std::filesystem::path imageFilepath;

	/// The unchanged decoded CPU image used for the built-in conversion reference.
	std::optional<fcg::Image> image;

	/// Pre-gamma floating-point linear RGB version of \ref image.
	std::optional<fcg::Image> linearImage;

	/// Reusable floating-point destination for CPU display encoding.
	std::optional<fcg::Image> correctedImage;

	/// Texture created from \ref image for displaying.
	std::optional<fcg::Texture> texture;

	/// Texture created from \ref correctedImage for displaying.
	std::optional<fcg::Texture> correctedTexture;

	/// The renderer for the image quad.
	std::optional<fcg::QuadRenderer> qr;

	/// Linear filtered, clamped image sampling.
	std::optional<fcg::Sampler> sampler;

	/// One image quad's transform.
	std::optional<fcg::PrimitiveAttributes> attributes;

	/// Light value distribution of the current image; independent of the gamma sliders.
	Histogram inputHistogram;

	/// Display code distribution at the gamma that is currently effective.
	Histogram displayHistogram;

	/// View settings of the histogram panel, such as logarithmic scaling.
	HistogramView histogramView;
};



//////
//
// Functions
//

/// Program entry point.
auto main () -> int {
	// Run with 2D camera and our image viewer applet
	return fcg::run<fcg::applet::Camera2D, HDRViewerApplet>(fcg::PlayerSettings {
		.mainWindowTitle = "FCG Intro Exercise - HDR and color spaces",
		.sceneRenderTargetFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT // <- make sure we can do proper HDR
	});
}
