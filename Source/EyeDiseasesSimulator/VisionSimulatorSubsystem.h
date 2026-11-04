#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisionCondition.h"
#include "VisionSimulatorSubsystem.generated.h"

class AVisionDisplayActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVisionConditionChangedSignature, EVisionCondition, Condition, float, Severity);

/**
 * Owns the simulator state (current condition and severity) for a game world
 * and spawns the AVisionDisplayActor that shows the processed camera image.
 * UI (hand menu) talks to this subsystem only.
 *
 * Console variables:
 *   vision.AutoCycleSeconds - switch to the next condition every N seconds (0 = off). Demo mode until the hand menu exists.
 *   vision.SpawnDisplay     - spawn the display actor on BeginPlay (1) or not (0).
 */
UCLASS()
class EYEDISEASESSIMULATOR_API UVisionSimulatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetCondition(EVisionCondition NewCondition);

	/** 0 = healthy, 1 = severe. */
	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetSeverity(float NewSeverity);

	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SelectNextCondition();

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	EVisionCondition GetCondition() const { return Condition; }

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	float GetSeverity() const { return Severity; }

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	AVisionDisplayActor* GetDisplayActor() const { return DisplayActor; }

	UPROPERTY(BlueprintAssignable, Category = "Vision Simulator")
	FVisionConditionChangedSignature OnConditionChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void BroadcastChange();

	UPROPERTY()
	TObjectPtr<AVisionDisplayActor> DisplayActor;

	EVisionCondition Condition = EVisionCondition::None;
	float Severity = 0.7f;

	FTimerHandle AutoCycleTimer;
};
