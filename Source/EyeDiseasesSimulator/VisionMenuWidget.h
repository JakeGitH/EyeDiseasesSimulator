#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisionCondition.h"
#include "VisionMenuWidget.generated.h"

class UVisionSimulatorSubsystem;

/** Page the hand menu shows; matches the child order of a Widget Switcher in WBP_HandMenu. */
UENUM(BlueprintType)
enum class EVisionMenuPage : uint8
{
	/** No condition active: list of conditions to pick from. */
	ConditionList,
	/** A condition without adjustable value is active: name, description and the Back button only. */
	ActiveCondition,
	/** Myopia / hyperopia / presbyopia: slider (diopters or age), sharp range info and the Back button. */
	AdjustCondition
};

/**
 * Base class of the hand menu widget (WBP_HandMenu). All logic lives here; the Blueprint only lays out the UI:
 *
 *   - Widget Switcher with 3 pages in EVisionMenuPage order; in OnMenuRefresh set its active index to (int)Page.
 *   - ConditionList page: one button per GetSelectableConditions() entry -> SelectCondition(Condition).
 *     Picking a condition switches to another page, so a stray touch cannot change the condition (the list
 *     is only reachable again through Back).
 *   - ActiveCondition page: GetConditionName / GetConditionDescription texts, Back button -> Back().
 *   - AdjustCondition page: Slider (Min/Max/Step from GetSliderRange, value GetSliderValue,
 *     OnValueChanged -> SetSliderValue), GetSliderValueText, GetSharpRangeText, Back button.
 *   - Optional: Compare toggle -> ToggleCompareMode (split screen: normal | impaired).
 *
 * OnMenuRefresh is called on construction and after every settings change; refresh texts and the page there.
 */
UCLASS(Abstract)
class EYEDISEASESSIMULATOR_API UVisionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vision Menu")
	void SelectCondition(EVisionCondition Condition);

	/** Turns the active condition off and returns to the condition list. */
	UFUNCTION(BlueprintCallable, Category = "Vision Menu")
	void Back();

	/** Diopters for myopia / hyperopia, age for presbyopia. Snapped to the slider step. */
	UFUNCTION(BlueprintCallable, Category = "Vision Menu")
	void SetSliderValue(float Value);

	UFUNCTION(BlueprintCallable, Category = "Vision Menu")
	void ToggleCompareMode();

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	EVisionMenuPage GetPage() const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	EVisionCondition GetCondition() const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	bool IsCompareMode() const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	FText GetConditionName() const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	FText GetConditionDescription() const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	void GetSliderRange(float& OutMin, float& OutMax, float& OutStep) const;

	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	float GetSliderValue() const;

	/** E.g. "-7.00 D" or "Age 60". */
	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	FText GetSliderValueText() const;

	/** E.g. "Sharp from 14 cm to 33 cm" or "Accommodation: 0.5 D". */
	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	FText GetSharpRangeText() const;

	/** Label of the slider: "Refractive error" / "Age". */
	UFUNCTION(BlueprintPure, Category = "Vision Menu")
	FText GetSliderLabel() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Update the switcher page and all texts. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vision Menu")
	void OnMenuRefresh(EVisionMenuPage Page);

	/** Slider ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision Menu")
	float DiopterStep = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision Menu")
	float MinAge = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision Menu")
	float MaxAge = 75.0f;

	/** Slider ends shown in the menu; the subsystem accepts up to -20 / +10 D. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision Menu")
	float MaxMyopiaDiopters = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vision Menu")
	float MaxHyperopiaDiopters = 6.0f;

private:
	UVisionSimulatorSubsystem* GetSimulator() const;

	UFUNCTION()
	void HandleSettingsChanged(const FVisionSettings& Settings);
};
