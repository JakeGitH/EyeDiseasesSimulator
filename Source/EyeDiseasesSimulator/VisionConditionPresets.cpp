#include "VisionCondition.h"

#include "VisionImageProcessing.h"

namespace
{
	using FColorMatrix = FVector3f[3];

	/**
	 * Color vision deficiency matrices for severity 1.0 (dichromacy), linear RGB.
	 * Source: G. M. Machado, M. M. Oliveira, L. A. F. Fernandes,
	 * "A Physiologically-based Model for Simulation of Color Vision Deficiency", IEEE TVCG 15(6), 2009.
	 * Lower severities are approximated by linear interpolation from the identity matrix
	 * (the paper also tabulates matrices for severities 0.1-0.9).
	 */
	const FColorMatrix ProtanopiaMatrix = {
		FVector3f( 0.152286f,  1.052583f, -0.204868f),
		FVector3f( 0.114503f,  0.786281f,  0.099216f),
		FVector3f(-0.003882f, -0.048116f,  1.051998f)
	};

	const FColorMatrix DeuteranopiaMatrix = {
		FVector3f( 0.367322f,  0.860646f, -0.227968f),
		FVector3f( 0.280085f,  0.672501f,  0.047413f),
		FVector3f(-0.011820f,  0.042940f,  0.968881f)
	};

	const FColorMatrix TritanopiaMatrix = {
		FVector3f( 1.255528f, -0.076749f, -0.178779f),
		FVector3f(-0.078411f,  0.930809f,  0.147602f),
		FVector3f( 0.004733f,  0.691367f,  0.303900f)
	};

	/** Every output channel = Rec. 709 luminance. */
	const FColorMatrix GrayscaleMatrix = {
		FVector3f(0.2126f, 0.7152f, 0.0722f),
		FVector3f(0.2126f, 0.7152f, 0.0722f),
		FVector3f(0.2126f, 0.7152f, 0.0722f)
	};

	void SetColorMatrix(FVisionEffectParams& Params, const FColorMatrix& Target, float Severity)
	{
		const FVector3f Identity[3] = {
			FVector3f(1.0f, 0.0f, 0.0f),
			FVector3f(0.0f, 1.0f, 0.0f),
			FVector3f(0.0f, 0.0f, 1.0f)
		};
		for (int32 Row = 0; Row < 3; ++Row)
		{
			Params.ColorMatrixRows[Row] = FMath::Lerp(Identity[Row], Target[Row], Severity);
		}
	}
}

float VisionOptics::AccommodationAmplitude(float Age)
{
	return FMath::Max(18.5f - 0.3f * Age, 0.0f);
}

float VisionOptics::RefractiveError(const FVisionSettings& Settings)
{
	switch (Settings.Condition)
	{
	case EVisionCondition::Myopia:
		return FMath::Min(Settings.MyopiaDiopters, 0.0f);
	case EVisionCondition::Hyperopia:
		return FMath::Max(Settings.HyperopiaDiopters, 0.0f);
	default:
		return 0.0f;
	}
}

void VisionOptics::FocusRange(const FVisionSettings& Settings, float& OutMinVergence, float& OutMaxVergence)
{
	OutMinVergence = -RefractiveError(Settings);
	OutMaxVergence = OutMinVergence + AccommodationAmplitude(Settings.Age);
}

float VisionOptics::Defocus(float Vergence, float MinVergence, float MaxVergence)
{
	return FMath::Max3(MinVergence - Vergence, Vergence - MaxVergence, 0.0f);
}

bool VisionOptics::UsesDepth(EVisionCondition Condition)
{
	return Condition == EVisionCondition::Myopia
		|| Condition == EVisionCondition::Hyperopia
		|| Condition == EVisionCondition::Presbyopia;
}

FVisionEffectParams VisionConditionPresets::MakeEffectParams(const FVisionSettings& Settings)
{
	const float S = FMath::Clamp(Settings.Severity, 0.0f, 1.0f);
	FVisionEffectParams Params;

	switch (Settings.Condition)
	{
	case EVisionCondition::Protanopia:
		// Missing / anomalous L-cones: red-green confusion, reds look dark.
		SetColorMatrix(Params, ProtanopiaMatrix, S);
		break;

	case EVisionCondition::Deuteranopia:
		// Missing / anomalous M-cones: red-green confusion (most common form).
		SetColorMatrix(Params, DeuteranopiaMatrix, S);
		break;

	case EVisionCondition::Tritanopia:
		// Missing / anomalous S-cones: blue-yellow confusion.
		SetColorMatrix(Params, TritanopiaMatrix, S);
		break;

	case EVisionCondition::Achromatopsia:
		// No functioning cones: no color, reduced acuity and sensitivity to bright light.
		SetColorMatrix(Params, GrayscaleMatrix, S);
		Params.BlurSigma = 3.0f * S;
		Params.UniformBlur = S;
		Params.GlareIntensity = 0.6f * S;
		Params.GlareThreshold = 0.75f;
		Params.GlareSigma = 10.0f;
		break;

	case EVisionCondition::Cataract:
		// Clouded lens: blur, light scattering (haze, glare around lights), yellow-brown tint, lower contrast.
		Params.BlurSigma = 1.5f + 8.0f * S;
		Params.UniformBlur = FMath::Min(1.0f, 2.0f * S);
		Params.HazeColor = FLinearColor(0.55f, 0.50f, 0.38f);
		Params.HazeAmount = 0.35f * S;
		Params.Tint = FMath::Lerp(FLinearColor::White, FLinearColor(1.0f, 0.88f, 0.62f), S);
		Params.Contrast = 1.0f - 0.25f * S;
		Params.GlareIntensity = 1.5f * S;
		Params.GlareThreshold = 0.6f;
		Params.GlareSigma = 12.0f + 20.0f * S;
		break;

	case EVisionCondition::Glaucoma:
		// Optic nerve damage: peripheral field loss progressing toward tunnel vision; center stays sharp.
		Params.VignetteInner = FMath::Lerp(1.3f, 0.25f, S);
		Params.VignetteOuter = Params.VignetteInner + FMath::Lerp(0.6f, 0.3f, S);
		Params.VignetteDarkness = FMath::Lerp(0.5f, 1.0f, S);
		Params.BlurSigma = 6.0f;
		Params.PeripheralBlur = 1.0f;
		Params.Contrast = 1.0f - 0.15f * S;
		break;

	case EVisionCondition::MacularDegeneration:
		// Damaged macula: dark / blurred central blind spot and distorted straight lines; periphery is intact.
		Params.ScotomaRadius = 0.05f + 0.25f * S;
		Params.ScotomaSoftness = 0.08f + 0.10f * S;
		Params.ScotomaDarkness = 0.6f + 0.35f * S;
		Params.BlurSigma = 6.0f;
		Params.CentralBlur = 1.0f;
		Params.Distortion = S;
		break;

	case EVisionCondition::Myopia:
		// Nearsightedness: the eye focuses in front of the retina. Objects beyond the far point 1 / |D| are blurred,
		// near objects stay sharp.
	case EVisionCondition::Hyperopia:
		// Farsightedness: the relaxed eye focuses behind the retina; accommodation compensates as long as it can,
		// so near objects (closer than the near point) are blurred first.
	case EVisionCondition::Presbyopia:
		// Age-related loss of accommodation: the near point moves away, near objects are blurred.
		// All three use the same thin-lens model (VisionOptics::FocusRange) and the per-pixel depth (vergence) map.
		Params.bDepthDefocus = true;
		VisionOptics::FocusRange(Settings, Params.FocusVergenceMin, Params.FocusVergenceMax);
		break;

	case EVisionCondition::DiabeticRetinopathy:
		// Damaged retinal vessels: dark patches / floaters, blurred and less contrasted vision.
		Params.SpotsAmount = S;
		Params.SpotsDarkness = 0.85f;
		Params.SpotsScale = 3.0f;
		Params.SpotsSeed = 11.0f;
		Params.BlurSigma = 2.0f + 4.0f * S;
		Params.UniformBlur = 0.5f * S;
		Params.Contrast = 1.0f - 0.2f * S;
		break;

	case EVisionCondition::None:
	case EVisionCondition::Count:
	default:
		break;
	}

	return Params;
}
