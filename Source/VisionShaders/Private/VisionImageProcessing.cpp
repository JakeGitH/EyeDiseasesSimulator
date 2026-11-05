#include "VisionImageProcessing.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

namespace
{
	constexpr int32 MaxBlurRadius = 16;
	constexpr int32 NumBlurWeightVectors = (MaxBlurRadius + 1 + 3) / 4;

	// Largest sigma (in pixels of the texture being blurred) that a 3-sigma kernel of MaxBlurRadius taps covers.
	constexpr float MaxSigmaPerPass = MaxBlurRadius / 3.0f;

	// Largest sigma the blur chain handles (8x downsample); stronger defocus is clamped to it.
	constexpr float MaxDefocusSigma = 8.0f * MaxSigmaPerPass;

	// Middle defocus level as a fraction of the strongest one. Pixels between levels are linearly interpolated.
	constexpr float DefocusMidLevelFraction = 0.35f;
}

// IMPLEMENT_GLOBAL_SHADER wraps the path in TEXT(), so it must be a plain string literal.
#define VISION_SHADER_FILE "/VisionShaders/Private/VisionEffects.usf"

/** Box-filtered downsample, optional sRGB decode and optional bright-pass (for glare). */
class FVisionDownsamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisionDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FVisionDownsamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector2f, OutputInvSize)
		SHADER_PARAMETER(float, BrightThreshold)
		SHADER_PARAMETER(uint32, bDecodeSourceSRGB)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

/** One direction of a separable Gaussian blur. */
class FVisionGaussianBlurPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisionGaussianBlurPS);
	SHADER_USE_PARAMETER_STRUCT(FVisionGaussianBlurPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurSourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurSourceSampler)
		SHADER_PARAMETER(FVector2f, BlurInvSize)
		SHADER_PARAMETER(FVector2f, BlurTexelStep)
		SHADER_PARAMETER_ARRAY(FVector4f, BlurWeights, [NumBlurWeightVectors])
		SHADER_PARAMETER(uint32, BlurRadius)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_BLUR_WEIGHT_VECTORS"), NumBlurWeightVectors);
	}
};

/** Final per-pixel pass: combines sharp, blurred and glare images and applies color, masks and noise. */
class FVisionCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisionCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FVisionCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GlareTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GlareSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredMidTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredMidSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VergenceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VergenceSampler)
		SHADER_PARAMETER(FVector2f, CompositeInvSize)
		SHADER_PARAMETER(float, AspectRatio)
		SHADER_PARAMETER(uint32, bDecodeInputSRGB)
		SHADER_PARAMETER(FVector4f, ColorRow0)
		SHADER_PARAMETER(FVector4f, ColorRow1)
		SHADER_PARAMETER(FVector4f, ColorRow2)
		SHADER_PARAMETER(FVector4f, BlurMix)
		SHADER_PARAMETER(FVector4f, GlareParams)
		SHADER_PARAMETER(FVector4f, HazeColorAndAmount)
		SHADER_PARAMETER(FVector4f, TintAndContrast)
		SHADER_PARAMETER(FVector4f, VignetteParams)
		SHADER_PARAMETER(FVector4f, ScotomaParams)
		SHADER_PARAMETER(FVector4f, SpotsParams)
		SHADER_PARAMETER(FVector4f, DefocusParams)
		SHADER_PARAMETER(FVector4f, DefocusLevels)
		SHADER_PARAMETER(FVector4f, OutputParams)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisionDownsamplePS, VISION_SHADER_FILE, "DownsamplePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVisionGaussianBlurPS, VISION_SHADER_FILE, "GaussianBlurPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVisionCompositePS, VISION_SHADER_FILE, "CompositePS", SF_Pixel);

namespace
{
	FRHISamplerState* GetBilinearClampSampler()
	{
		return TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	FRDGTextureRef CreateIntermediateTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent, const TCHAR* Name)
	{
		// Half-float keeps dark gradients free of banding after blurring in linear space.
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			Extent,
			PF_FloatRGBA,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable);
		return GraphBuilder.CreateTexture(Desc, Name);
	}

	/**
	 * Normalized 1D Gaussian weights w[i] = exp(-i^2 / (2 sigma^2)) for i = 0..Radius, packed 4 per vector.
	 * The kernel is symmetric, so only the center and one side are stored.
	 */
	int32 ComputeGaussianWeights(float Sigma, FVector4f (&OutWeights)[NumBlurWeightVectors])
	{
		const int32 Radius = FMath::Clamp(FMath::CeilToInt(Sigma * 3.0f), 1, MaxBlurRadius);

		float Weights[NumBlurWeightVectors * 4] = {};
		float Sum = 0.0f;
		for (int32 Index = 0; Index <= Radius; ++Index)
		{
			Weights[Index] = FMath::Exp(-static_cast<float>(Index * Index) / (2.0f * Sigma * Sigma));
			Sum += (Index == 0) ? Weights[Index] : 2.0f * Weights[Index];
		}

		for (int32 Vector = 0; Vector < NumBlurWeightVectors; ++Vector)
		{
			OutWeights[Vector] = FVector4f(
				Weights[Vector * 4 + 0] / Sum,
				Weights[Vector * 4 + 1] / Sum,
				Weights[Vector * 4 + 2] / Sum,
				Weights[Vector * 4 + 3] / Sum);
		}
		return Radius;
	}

	void AddGaussianBlurPass(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* ShaderMap,
		FRDGTextureRef Source,
		FRDGTextureRef Destination,
		FVector2f Direction,
		float Sigma)
	{
		const FIntPoint Extent = Destination->Desc.Extent;
		const FVector2f InvSize(1.0f / Extent.X, 1.0f / Extent.Y);

		FVisionGaussianBlurPS::FParameters* Parameters = GraphBuilder.AllocParameters<FVisionGaussianBlurPS::FParameters>();
		Parameters->BlurSourceTexture = Source;
		Parameters->BlurSourceSampler = GetBilinearClampSampler();
		Parameters->BlurInvSize = InvSize;
		Parameters->BlurTexelStep = Direction * InvSize;
		FVector4f Weights[NumBlurWeightVectors];
		Parameters->BlurRadius = static_cast<uint32>(ComputeGaussianWeights(Sigma, Weights));
		for (int32 Index = 0; Index < NumBlurWeightVectors; ++Index)
		{
			Parameters->BlurWeights[Index] = Weights[Index];
		}
		Parameters->RenderTargets[0] = FRenderTargetBinding(Destination, ERenderTargetLoadAction::ENoAction);

		TShaderMapRef<FVisionGaussianBlurPS> PixelShader(ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			ShaderMap,
			RDG_EVENT_NAME("VisionGaussianBlur %s %dx%d", Direction.X > 0.0f ? TEXT("H") : TEXT("V"), Extent.X, Extent.Y),
			PixelShader,
			Parameters,
			FIntRect(FIntPoint::ZeroValue, Extent));
	}

	/**
	 * Downsample -> horizontal blur -> vertical blur.
	 * The image is first reduced by a power of two so the kernel stays within MaxBlurRadius taps;
	 * blurring a downsampled image with sigma/Factor is close to blurring the full image with sigma, and much cheaper.
	 * A non-negative BrightThreshold keeps only bright areas (used for glare).
	 */
	FRDGTextureRef AddBlurChain(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* ShaderMap,
		FRDGTextureRef Input,
		float Sigma,
		float BrightThreshold,
		bool bDecodeInputSRGB,
		const TCHAR* Name)
	{
		int32 Factor = 1;
		while (Sigma / Factor > MaxSigmaPerPass && Factor < 8)
		{
			Factor *= 2;
		}
		const float ScaledSigma = FMath::Max(Sigma / Factor, 0.5f);

		const FIntPoint InputExtent = Input->Desc.Extent;
		const FIntPoint Extent(
			FMath::Max(FMath::DivideAndRoundUp(InputExtent.X, Factor), 1),
			FMath::Max(FMath::DivideAndRoundUp(InputExtent.Y, Factor), 1));

		FRDGTextureRef Downsampled = CreateIntermediateTexture(GraphBuilder, Extent, Name);
		{
			FVisionDownsamplePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVisionDownsamplePS::FParameters>();
			Parameters->SourceTexture = Input;
			Parameters->SourceSampler = GetBilinearClampSampler();
			Parameters->OutputInvSize = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);
			Parameters->BrightThreshold = BrightThreshold;
			Parameters->bDecodeSourceSRGB = bDecodeInputSRGB ? 1u : 0u;
			Parameters->RenderTargets[0] = FRenderTargetBinding(Downsampled, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FVisionDownsamplePS> PixelShader(ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("VisionDownsample x%d %dx%d", Factor, Extent.X, Extent.Y),
				PixelShader,
				Parameters,
				FIntRect(FIntPoint::ZeroValue, Extent));
		}

		FRDGTextureRef Horizontal = CreateIntermediateTexture(GraphBuilder, Extent, Name);
		AddGaussianBlurPass(GraphBuilder, ShaderMap, Downsampled, Horizontal, FVector2f(1.0f, 0.0f), ScaledSigma);

		// The downsampled texture is no longer needed after the horizontal pass; RDG reuses its memory for the result.
		FRDGTextureRef Vertical = CreateIntermediateTexture(GraphBuilder, Extent, Name);
		AddGaussianBlurPass(GraphBuilder, ShaderMap, Horizontal, Vertical, FVector2f(0.0f, 1.0f), ScaledSigma);

		return Vertical;
	}

	FVector4f ToVector4(const FVector3f& Vector, float W = 0.0f)
	{
		return FVector4f(Vector.X, Vector.Y, Vector.Z, W);
	}
}

void VisionImageProcessing::AddVisionEffectPasses(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputTexture,
	FRDGTextureRef OutputTexture,
	const FVisionEffectParams& Params,
	FRDGTextureRef VergenceTexture)
{
	check(IsInRenderingThread());
	check(InputTexture && OutputTexture);

	RDG_EVENT_SCOPE(GraphBuilder, "VisionEffects");

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	const bool bNeedsBlur = Params.BlurSigma > 0.0f
		&& (Params.UniformBlur > 0.0f || Params.PeripheralBlur > 0.0f || Params.CentralBlur > 0.0f);
	const bool bNeedsGlare = Params.GlareIntensity > 0.0f && Params.GlareSigma > 0.0f;

	// Depth-dependent defocus replaces the fixed blur: two blur levels (mid, max) for the strongest defocus in the frame.
	const float DefocusSigmaMax = FMath::Min(Params.MaxDefocusDiopters * Params.BlurSigmaPerDiopter, MaxDefocusSigma);
	const bool bNeedsDefocus = Params.bDepthDefocus && VergenceTexture && DefocusSigmaMax >= 0.5f;
	const float DefocusSigmaMid = FMath::Max(DefocusSigmaMax * DefocusMidLevelFraction, 0.5f);

	// Unused inputs are bound to the camera frame itself; their blend weights are zero.
	FRDGTextureRef Blurred = InputTexture;
	FRDGTextureRef BlurredMid = InputTexture;
	if (bNeedsDefocus)
	{
		Blurred = AddBlurChain(GraphBuilder, ShaderMap, InputTexture, DefocusSigmaMax, -1.0f, Params.bDecodeInputSRGB, TEXT("VisionDefocusMax"));
		BlurredMid = AddBlurChain(GraphBuilder, ShaderMap, InputTexture, DefocusSigmaMid, -1.0f, Params.bDecodeInputSRGB, TEXT("VisionDefocusMid"));
	}
	else if (bNeedsBlur)
	{
		Blurred = AddBlurChain(GraphBuilder, ShaderMap, InputTexture, Params.BlurSigma, -1.0f, Params.bDecodeInputSRGB, TEXT("VisionBlur"));
	}
	FRDGTextureRef Glare = bNeedsGlare
		? AddBlurChain(GraphBuilder, ShaderMap, InputTexture, Params.GlareSigma, Params.GlareThreshold, Params.bDecodeInputSRGB, TEXT("VisionGlare"))
		: InputTexture;

	const FIntPoint Extent = OutputTexture->Desc.Extent;

	FVisionCompositePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVisionCompositePS::FParameters>();
	Parameters->InputTexture = InputTexture;
	Parameters->InputSampler = GetBilinearClampSampler();
	Parameters->BlurredTexture = Blurred;
	Parameters->BlurredSampler = GetBilinearClampSampler();
	Parameters->GlareTexture = Glare;
	Parameters->GlareSampler = GetBilinearClampSampler();
	Parameters->BlurredMidTexture = BlurredMid;
	Parameters->BlurredMidSampler = GetBilinearClampSampler();
	Parameters->VergenceTexture = VergenceTexture ? VergenceTexture : InputTexture;
	Parameters->VergenceSampler = GetBilinearClampSampler();
	Parameters->CompositeInvSize = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);
	Parameters->AspectRatio = static_cast<float>(Extent.X) / Extent.Y;
	Parameters->bDecodeInputSRGB = Params.bDecodeInputSRGB ? 1u : 0u;
	Parameters->ColorRow0 = ToVector4(Params.ColorMatrixRows[0]);
	Parameters->ColorRow1 = ToVector4(Params.ColorMatrixRows[1]);
	Parameters->ColorRow2 = ToVector4(Params.ColorMatrixRows[2]);
	Parameters->BlurMix = (bNeedsBlur && !bNeedsDefocus)
		? FVector4f(Params.UniformBlur, Params.PeripheralBlur, Params.CentralBlur, 0.0f)
		: FVector4f::Zero();
	Parameters->GlareParams = FVector4f(bNeedsGlare ? Params.GlareIntensity : 0.0f, 0.0f, 0.0f, 0.0f);
	Parameters->HazeColorAndAmount = FVector4f(Params.HazeColor.R, Params.HazeColor.G, Params.HazeColor.B, Params.HazeAmount);
	Parameters->TintAndContrast = FVector4f(Params.Tint.R, Params.Tint.G, Params.Tint.B, Params.Contrast);
	Parameters->VignetteParams = FVector4f(Params.VignetteInner, Params.VignetteOuter, Params.VignetteDarkness, Params.Brightness);
	Parameters->ScotomaParams = FVector4f(Params.ScotomaRadius, Params.ScotomaSoftness, Params.ScotomaDarkness, Params.Distortion);
	Parameters->SpotsParams = FVector4f(Params.SpotsAmount, Params.SpotsDarkness, Params.SpotsScale, Params.SpotsSeed);
	Parameters->DefocusParams = FVector4f(Params.FocusVergenceMin, Params.FocusVergenceMax, Params.BlurSigmaPerDiopter, bNeedsDefocus ? 1.0f : 0.0f);
	Parameters->DefocusLevels = FVector4f(DefocusSigmaMid, DefocusSigmaMax, VergenceTexture ? 1.0f : 0.0f, 0.0f);
	Parameters->OutputParams = FVector4f(Params.CompareSplit, static_cast<float>(Params.DebugView), 0.0f, 0.0f);
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);

	TShaderMapRef<FVisionCompositePS> PixelShader(ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("VisionComposite %dx%d", Extent.X, Extent.Y),
		PixelShader,
		Parameters,
		FIntRect(FIntPoint::ZeroValue, Extent));
}
