#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

/**
 * Parameters of the image processing chain applied to one camera frame.
 * Every field has a neutral default, so a default-constructed struct leaves the image unchanged.
 *
 * Radial values (vignette, scotoma) are measured from the image center in units of half the image height:
 * 0 = center, 1 = top/bottom edge, ~1.33 = left/right edge for a 4:3 frame.
 */
struct FVisionEffectParams
{
	/** 3x3 color transform applied in linear RGB: Out = ColorMatrix * In. Rows are stored in X, Y, Z of each row vector. */
	FVector3f ColorMatrixRows[3] = {
		FVector3f(1.0f, 0.0f, 0.0f),
		FVector3f(0.0f, 1.0f, 0.0f),
		FVector3f(0.0f, 0.0f, 1.0f)
	};

	/** Standard deviation of the Gaussian blur, in full-resolution pixels. 0 disables the blur chain. */
	float BlurSigma = 0.0f;

	/** How much of the blurred image is mixed in everywhere (0-1). */
	float UniformBlur = 0.0f;

	/** How much of the blurred image is mixed in on the periphery, using the vignette radii (0-1). */
	float PeripheralBlur = 0.0f;

	/** How much of the blurred image is mixed in around the center, using the scotoma radius (0-1). */
	float CentralBlur = 0.0f;

	/** Strength of the glare (bloom) added around bright areas. 0 disables the glare chain. */
	float GlareIntensity = 0.0f;

	/** Luminance above which a pixel produces glare (0-1). */
	float GlareThreshold = 0.8f;

	/** Gaussian sigma of the glare, in full-resolution pixels. */
	float GlareSigma = 16.0f;

	/** Veiling haze: the image is blended toward HazeColor by HazeAmount (0-1). Lowers contrast like light scattering. */
	FLinearColor HazeColor = FLinearColor(0.5f, 0.5f, 0.5f);
	float HazeAmount = 0.0f;

	/** Contrast around linear mid-grey (0.18). 1 = unchanged. */
	float Contrast = 1.0f;

	/** Per-channel multiplier (e.g. yellowing of the lens). */
	FLinearColor Tint = FLinearColor::White;

	/** Global brightness multiplier. */
	float Brightness = 1.0f;

	/** Peripheral mask: 0 inside VignetteInner, 1 outside VignetteOuter, smooth in between. */
	float VignetteInner = 10.0f;
	float VignetteOuter = 11.0f;

	/** How dark the masked periphery gets (0-1). */
	float VignetteDarkness = 0.0f;

	/** Central blind spot radius. */
	float ScotomaRadius = 0.0f;

	/** Width of the soft edge of the scotoma. */
	float ScotomaSoftness = 0.1f;

	/** How dark the scotoma is (0-1). 0 disables it. */
	float ScotomaDarkness = 0.0f;

	/** Wavy distortion of straight lines around the center (metamorphopsia), 0-1. */
	float Distortion = 0.0f;

	/** Amount of dark blotches (0-1). 0 disables them. */
	float SpotsAmount = 0.0f;

	/** How dark the blotches are (0-1). */
	float SpotsDarkness = 0.8f;

	/** Noise frequency of the blotches; larger = smaller and more numerous spots. */
	float SpotsScale = 6.0f;

	/** Seed that moves the blotch pattern. */
	float SpotsSeed = 0.0f;

	/**
	 * Depth-dependent defocus (refractive errors). Needs the vergence map passed to AddVisionEffectPasses:
	 * a texture with 1 / distance (diopters) for every image position.
	 * A pixel is blurred with sigma = BlurSigmaPerDiopter * Defocus, where Defocus is how far its vergence lies
	 * outside [FocusVergenceMin, FocusVergenceMax], the range the simulated eye can focus on.
	 */
	bool bDepthDefocus = false;
	float FocusVergenceMin = 0.0f;
	float FocusVergenceMax = 0.0f;

	/** Gaussian sigma (full-resolution pixels) caused by one diopter of defocus. Depends on pupil size and camera focal length. */
	float BlurSigmaPerDiopter = 0.0f;

	/** Largest defocus (diopters) present in the frame. Sets the strongest of the precomputed blur levels. */
	float MaxDefocusDiopters = 0.0f;

	/** Split view for comparison: pixels left of this horizontal position (0-1) stay unprocessed. Negative disables it. */
	float CompareSplit = -1.0f;

	/** Debug output: 0 = normal, 1 = vergence map (depth) as grayscale, 2 = per-pixel defocus blur sigma. */
	int32 DebugView = 0;

	/** Set when the input texture holds sRGB-encoded values but is sampled without hardware sRGB decoding. */
	bool bDecodeInputSRGB = false;
};

namespace VisionImageProcessing
{
	/**
	 * Adds all passes that turn InputTexture (camera frame) into OutputTexture (simulated impaired vision).
	 * Must be called on the render thread. OutputTexture must be render-targetable.
	 * VergenceTexture (optional) holds 1 / distance in diopters, in the same UV space as InputTexture;
	 * it is required for Params.bDepthDefocus.
	 */
	VISIONSHADERS_API void AddVisionEffectPasses(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef InputTexture,
		FRDGTextureRef OutputTexture,
		const FVisionEffectParams& Params,
		FRDGTextureRef VergenceTexture = nullptr);
}
