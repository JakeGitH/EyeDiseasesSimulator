#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "VisionMenuWidget.h"
#include "VisionHandMenuWidget.generated.h"

class UBorder;
class USlider;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

/** Menu tile that selects one vision condition. */
UCLASS()
class EYEDISEASESSIMULATOR_API UVisionConditionButton : public UButton
{
	GENERATED_BODY()

public:
	void Setup(UVisionMenuWidget* InMenu, EVisionCondition InCondition);

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UVisionMenuWidget> Menu;
	EVisionCondition Condition = EVisionCondition::None;
};

/**
 * Hand menu with the layout and style built in C++ (no Widget Blueprint needed).
 *
 *   +----------------------------+
 *   | VISION SIMULATOR           |  header
 *   | <condition name>           |  status
 *   | -------------------------- |
 *   | page (widget switcher):    |
 *   |  0 ConditionList   2x6 grid of condition tiles
 *   |  1 ActiveCondition description + Back
 *   |  2 AdjustCondition value, [-] slider [+], sharp range, description + Back
 *   |  3 ConfirmExit     "Exit the app?" [Cancel] [Exit]
 *   | [ Compare: OFF ] [ Exit ]  |  footer (hidden on the confirmation page)
 *   +----------------------------+
 *
 * Size: MenuSize pixels; the hand menu actor's DrawSize should match.
 */
UCLASS()
class EYEDISEASESSIMULATOR_API UVisionHandMenuWidget : public UVisionMenuWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	static constexpr float MenuWidth = 500.0f;
	static constexpr float MenuHeight = 700.0f;

protected:
	virtual void NativeOnMenuRefresh(EVisionMenuPage Page) override;

private:
	void BuildLayout();
	UWidget* BuildConditionListPage();
	UWidget* BuildActiveConditionPage();
	UWidget* BuildAdjustConditionPage();
	UWidget* BuildConfirmExitPage();

	UButton* MakeButton(const FText& Label, float Height, bool bDanger, UTextBlock** OutLabel = nullptr);
	UTextBlock* MakeText(const FText& Text, int32 Size, bool bBold, const FLinearColor& Color);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleMinusClicked();

	UFUNCTION()
	void HandlePlusClicked();

	UFUNCTION()
	void HandleCompareClicked();

	UFUNCTION()
	void HandleExitClicked();

	UFUNCTION()
	void HandleExitConfirmed();

	UFUNCTION()
	void HandleExitCancelled();

	UFUNCTION()
	void HandleSliderChanged(float Value);

	void StepSlider(float Direction);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> ConditionGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActiveDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SliderLabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SliderValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ValueSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SharpRangeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AdjustDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CompareText;

	/** Compare + Exit row, hidden on the exit confirmation page. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> Footer;
};
