#include "VisionMenuWidget.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "VisionConditionLibrary.h"
#include "VisionSimulatorSubsystem.h"

#define LOCTEXT_NAMESPACE "VisionMenu"

UVisionSimulatorSubsystem* UVisionMenuWidget::GetSimulator() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UVisionSimulatorSubsystem>() : nullptr;
}

void UVisionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UVisionSimulatorSubsystem* Simulator = GetSimulator())
	{
		Simulator->OnSettingsChanged.AddUniqueDynamic(this, &UVisionMenuWidget::HandleSettingsChanged);
	}
	RefreshMenu();
}

void UVisionMenuWidget::RefreshMenu()
{
	const EVisionMenuPage Page = GetPage();
	NativeOnMenuRefresh(Page);
	OnMenuRefresh(Page);
}

void UVisionMenuWidget::NativeDestruct()
{
	if (UVisionSimulatorSubsystem* Simulator = GetSimulator())
	{
		Simulator->OnSettingsChanged.RemoveDynamic(this, &UVisionMenuWidget::HandleSettingsChanged);
	}
	Super::NativeDestruct();
}

void UVisionMenuWidget::HandleSettingsChanged(const FVisionSettings& Settings)
{
	RefreshMenu();
}

void UVisionMenuWidget::SelectCondition(EVisionCondition Condition)
{
	// Only from the list: an active condition must be turned off with Back first.
	if (GetPage() != EVisionMenuPage::ConditionList)
	{
		return;
	}
	if (UVisionSimulatorSubsystem* Simulator = GetSimulator())
	{
		Simulator->SetCondition(Condition);
	}
}

void UVisionMenuWidget::Back()
{
	if (UVisionSimulatorSubsystem* Simulator = GetSimulator())
	{
		Simulator->ClearCondition();
	}
}

void UVisionMenuWidget::SetSliderValue(float Value)
{
	UVisionSimulatorSubsystem* Simulator = GetSimulator();
	if (!Simulator)
	{
		return;
	}

	float Min = 0.0f;
	float Max = 0.0f;
	float Step = 0.0f;
	GetSliderRange(Min, Max, Step);
	if (Step > 0.0f)
	{
		Value = FMath::GridSnap(Value, Step);
	}
	Value = FMath::Clamp(Value, Min, Max);

	switch (UVisionConditionLibrary::GetConditionControl(Simulator->GetCondition()))
	{
	case EVisionConditionControl::RefractiveError:
		Simulator->SetRefractiveError(Value);
		break;
	case EVisionConditionControl::Age:
		Simulator->SetAge(Value);
		break;
	default:
		break;
	}
}

void UVisionMenuWidget::ToggleCompareMode()
{
	if (UVisionSimulatorSubsystem* Simulator = GetSimulator())
	{
		Simulator->SetCompareMode(!Simulator->GetSettings().bCompareMode);
	}
}

EVisionCondition UVisionMenuWidget::GetCondition() const
{
	const UVisionSimulatorSubsystem* Simulator = GetSimulator();
	return Simulator ? Simulator->GetCondition() : EVisionCondition::None;
}

void UVisionMenuWidget::RequestExit()
{
	bExitConfirmPending = true;
	RefreshMenu();
}

void UVisionMenuWidget::CancelExit()
{
	bExitConfirmPending = false;
	RefreshMenu();
}

void UVisionMenuWidget::ConfirmExit()
{
	if (!bExitConfirmPending)
	{
		return;
	}
	const UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
}

EVisionMenuPage UVisionMenuWidget::GetPage() const
{
	if (bExitConfirmPending)
	{
		return EVisionMenuPage::ConfirmExit;
	}
	const EVisionCondition Condition = GetCondition();
	if (Condition == EVisionCondition::None)
	{
		return EVisionMenuPage::ConditionList;
	}
	return UVisionConditionLibrary::GetConditionControl(Condition) == EVisionConditionControl::None
		? EVisionMenuPage::ActiveCondition
		: EVisionMenuPage::AdjustCondition;
}

bool UVisionMenuWidget::IsCompareMode() const
{
	const UVisionSimulatorSubsystem* Simulator = GetSimulator();
	return Simulator && Simulator->GetSettings().bCompareMode;
}

FText UVisionMenuWidget::GetConditionName() const
{
	return UVisionConditionLibrary::GetConditionName(GetCondition());
}

FText UVisionMenuWidget::GetConditionDescription() const
{
	return UVisionConditionLibrary::GetConditionDescription(GetCondition());
}

void UVisionMenuWidget::GetSliderRange(float& OutMin, float& OutMax, float& OutStep) const
{
	OutMin = 0.0f;
	OutMax = 1.0f;
	OutStep = 0.0f;
	switch (GetCondition())
	{
	case EVisionCondition::Myopia:
		OutMin = -MaxMyopiaDiopters;
		OutMax = -DiopterStep;
		OutStep = DiopterStep;
		break;
	case EVisionCondition::Hyperopia:
		OutMin = DiopterStep;
		OutMax = MaxHyperopiaDiopters;
		OutStep = DiopterStep;
		break;
	case EVisionCondition::Presbyopia:
		OutMin = MinAge;
		OutMax = MaxAge;
		OutStep = 1.0f;
		break;
	default:
		break;
	}
}

float UVisionMenuWidget::GetSliderValue() const
{
	const UVisionSimulatorSubsystem* Simulator = GetSimulator();
	if (!Simulator)
	{
		return 0.0f;
	}
	switch (UVisionConditionLibrary::GetConditionControl(Simulator->GetCondition()))
	{
	case EVisionConditionControl::RefractiveError:
		return Simulator->GetRefractiveError();
	case EVisionConditionControl::Age:
		return Simulator->GetSettings().Age;
	default:
		return 0.0f;
	}
}

FText UVisionMenuWidget::GetSliderLabel() const
{
	switch (UVisionConditionLibrary::GetConditionControl(GetCondition()))
	{
	case EVisionConditionControl::RefractiveError:
		return LOCTEXT("RefractiveErrorLabel", "Refractive error");
	case EVisionConditionControl::Age:
		return LOCTEXT("AgeLabel", "Age");
	default:
		return FText::GetEmpty();
	}
}

FText UVisionMenuWidget::GetSliderValueText() const
{
	const float Value = GetSliderValue();
	switch (UVisionConditionLibrary::GetConditionControl(GetCondition()))
	{
	case EVisionConditionControl::RefractiveError:
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		Options.AlwaysSign = true;
		return FText::Format(LOCTEXT("DioptersValue", "{0} D"), FText::AsNumber(Value, &Options));
	}
	case EVisionConditionControl::Age:
		return FText::Format(LOCTEXT("AgeValue", "{0} years"), FText::AsNumber(FMath::RoundToInt(Value)));
	default:
		return FText::GetEmpty();
	}
}

FText UVisionMenuWidget::GetSharpRangeText() const
{
	const UVisionSimulatorSubsystem* Simulator = GetSimulator();
	if (!Simulator || !VisionOptics::UsesDepth(Simulator->GetCondition()))
	{
		return FText::GetEmpty();
	}

	float NearPoint = 0.0f;
	float FarPoint = 0.0f;
	if (!Simulator->GetSharpRange(NearPoint, FarPoint))
	{
		return LOCTEXT("NothingSharp", "Nothing is sharp at any distance");
	}
	if (NearPoint < 0.0f)
	{
		return LOCTEXT("OnlyFarSharp", "Only very distant objects are sharp");
	}

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 1;
	const FText Accommodation = FText::AsNumber(VisionOptics::AccommodationAmplitude(Simulator->GetSettings().Age), &Options);

	return FText::Format(LOCTEXT("SharpRange", "Sharp from {0} to {1}\nAccommodation {2} D (age {3})"),
		UVisionConditionLibrary::FormatDistance(NearPoint),
		UVisionConditionLibrary::FormatDistance(FarPoint),
		Accommodation,
		FText::AsNumber(FMath::RoundToInt(Simulator->GetSettings().Age)));
}

#undef LOCTEXT_NAMESPACE
