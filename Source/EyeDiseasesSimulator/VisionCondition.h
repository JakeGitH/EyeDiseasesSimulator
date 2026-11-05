#pragma once

#include "CoreMinimal.h"
#include "VisionCondition.generated.h"

struct FVisionEffectParams;

/** Vision impairments that the simulator can apply to the camera image. */
UENUM(BlueprintType)
enum class EVisionCondition : uint8
{
	None,
	Protanopia,
	Deuteranopia,
	Tritanopia,
	Achromatopsia,
	Cataract,
	Glaucoma,
	MacularDegeneration UMETA(DisplayName = "Macular Degeneration (AMD)"),
	Myopia,
	Hyperopia,
	Presbyopia,
	DiabeticRetinopathy UMETA(DisplayName = "Diabetic Retinopathy"),

	Count UMETA(Hidden)
};

/** Which value the menu lets the user adjust for a condition. */
UENUM(BlueprintType)
enum class EVisionConditionControl : uint8
{
	/** Nothing to adjust (the condition is shown with its default severity). */
	None,
	/** Refractive error in diopters (myopia: negative, hyperopia: positive). */
	RefractiveError,
	/** Age in years; drives the accommodation amplitude (presbyopia). */
	Age
};

/** Complete simulator state. Everything the image processing needs to know about the simulated eye. */
USTRUCT(BlueprintType)
struct FVisionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator")
	EVisionCondition Condition = EVisionCondition::None;

	/** 0 = healthy, 1 = severe. Used by conditions without a physical parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Severity = 0.7f;

	/** Myopia strength in diopters (negative, e.g. -7). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator")
	float MyopiaDiopters = -3.0f;

	/** Hyperopia strength in diopters (positive, e.g. +4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator")
	float HyperopiaDiopters = 3.0f;

	/**
	 * Age of the simulated person. Sets how much the lens can accommodate (focus on near objects),
	 * which matters for hyperopia and presbyopia.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator", meta = (ClampMin = "10.0", ClampMax = "80.0"))
	float Age = 45.0f;

	/** Split view: left half of the image unprocessed, right half processed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision Simulator")
	bool bCompareMode = false;
};

namespace VisionOptics
{
	/**
	 * Average amplitude of accommodation (diopters) for an age, Hofstetter's formula: A = 18.5 - 0.3 * age.
	 * (Hofstetter H. W., "Useful age-amplitude formula", Optometry World 38, 1950.)
	 */
	float AccommodationAmplitude(float Age);

	/** Refractive error (diopters) of the current condition: negative for myopia, positive for hyperopia, else 0. */
	float RefractiveError(const FVisionSettings& Settings);

	/**
	 * Range of object vergences (1 / distance in meters) the simulated eye can focus on:
	 * relaxed eye focuses at vergence -RefractiveError, full accommodation adds AccommodationAmplitude.
	 *   Min = -RefractiveError,  Max = -RefractiveError + AccommodationAmplitude
	 */
	void FocusRange(const FVisionSettings& Settings, float& OutMinVergence, float& OutMaxVergence);

	/** Defocus (diopters) of an object at the given vergence: its distance from the focus range, 0 inside it. */
	float Defocus(float Vergence, float MinVergence, float MaxVergence);

	/** True when the condition blurs by depth (refractive errors). */
	bool UsesDepth(EVisionCondition Condition);
}

namespace VisionConditionPresets
{
	/**
	 * Maps the simulator settings to image processing parameters.
	 * The mapping is documented per condition in VisionConditionPresets.cpp.
	 * Camera-dependent values (blur per diopter, depth-based blur levels) are filled in by AVisionDisplayActor.
	 */
	FVisionEffectParams MakeEffectParams(const FVisionSettings& Settings);
}
