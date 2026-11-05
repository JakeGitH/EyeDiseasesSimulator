#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisionCondition.h"
#include "VisionConditionLibrary.generated.h"

/** UI information about vision conditions (names, descriptions, what can be adjusted). */
UCLASS()
class EYEDISEASESSIMULATOR_API UVisionConditionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** All conditions the user can pick in the menu, in menu order (without None). */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	static TArray<EVisionCondition> GetSelectableConditions();

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	static FText GetConditionName(EVisionCondition Condition);

	/** One or two sentences: what the condition is and what the simulation shows. */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	static FText GetConditionDescription(EVisionCondition Condition);

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	static EVisionConditionControl GetConditionControl(EVisionCondition Condition);

	/** "45 cm", "1.5 m"; negative distance = "infinity". */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	static FText FormatDistance(float Meters);
};
