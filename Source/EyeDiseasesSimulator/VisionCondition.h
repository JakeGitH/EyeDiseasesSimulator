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
	DiabeticRetinopathy UMETA(DisplayName = "Diabetic Retinopathy"),

	Count UMETA(Hidden)
};

namespace VisionConditionPresets
{
	/**
	 * Maps a condition and its severity (0 = healthy, 1 = severe) to image processing parameters.
	 * The mapping is documented per condition in VisionConditionPresets.cpp.
	 */
	FVisionEffectParams MakeEffectParams(EVisionCondition Condition, float Severity);
}
