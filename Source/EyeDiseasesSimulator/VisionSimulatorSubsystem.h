#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisionCondition.h"
#include "VisionSimulatorSubsystem.generated.h"

class AHandMenuActor;
class AVisionDisplayActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVisionSettingsChangedSignature, const FVisionSettings&, Settings);

/**
 * Owns the simulator state (FVisionSettings) for a game world, spawns the AVisionDisplayActor that shows
 * the processed camera image and the hand menu. UI talks to this subsystem only.
 * The app starts with normal vision (EVisionCondition::None).
 *
 * Console variables:
 *   vision.AutoCycleSeconds - switch to the next condition every N seconds (0 = off). Demo / testing without the menu.
 *   vision.SpawnDisplay     - spawn the display actor on BeginPlay (1) or not (0).
 *   vision.SpawnHandMenu    - spawn the hand menu on BeginPlay (1) or not (0).
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

	/** Back to normal vision. */
	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void ClearCondition() { SetCondition(EVisionCondition::None); }

	/** 0 = healthy, 1 = severe. */
	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetSeverity(float NewSeverity);

	/**
	 * Refractive error of the current condition in diopters.
	 * Myopia: clamped to [-MaxMyopia, -0.25], hyperopia: [+0.25, +MaxHyperopia]. Ignored for other conditions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetRefractiveError(float Diopters);

	/** Age of the simulated person (years). Sets the accommodation amplitude (hyperopia, presbyopia). */
	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetAge(float NewAge);

	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SetCompareMode(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Vision Simulator")
	void SelectNextCondition();

	const FVisionSettings& GetSettings() const { return Settings; }

	UFUNCTION(BlueprintPure, Category = "Vision Simulator", meta = (DisplayName = "Get Settings"))
	FVisionSettings GetSettingsCopy() const { return Settings; }

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	EVisionCondition GetCondition() const { return Settings.Condition; }

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	float GetSeverity() const { return Settings.Severity; }

	/** Refractive error of the current condition (diopters), 0 when the condition is not myopia / hyperopia. */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	float GetRefractiveError() const;

	/** Allowed refractive error range of the current condition (for the menu slider). */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	void GetRefractiveErrorRange(float& OutMin, float& OutMax) const;

	/**
	 * Distances (meters) between which the simulated eye sees sharply: near point and far point.
	 * A negative value means infinity. Returns false when nothing is sharp at any distance (strong hyperopia).
	 */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	bool GetSharpRange(float& OutNearPoint, float& OutFarPoint) const;

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	AVisionDisplayActor* GetDisplayActor() const { return DisplayActor; }

	UPROPERTY(BlueprintAssignable, Category = "Vision Simulator")
	FVisionSettingsChangedSignature OnSettingsChanged;

	static constexpr float MaxMyopiaDiopters = 20.0f;
	static constexpr float MaxHyperopiaDiopters = 10.0f;
	static constexpr float MinRefractiveErrorDiopters = 0.25f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SpawnHandMenu(UWorld& InWorld);
	void BroadcastChange();

	UPROPERTY()
	TObjectPtr<AVisionDisplayActor> DisplayActor;

	UPROPERTY()
	TObjectPtr<AHandMenuActor> HandMenu;

	FVisionSettings Settings;

	FTimerHandle AutoCycleTimer;
};
