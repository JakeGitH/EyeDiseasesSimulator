#include "VisionSimulatorSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EyeDiseasesSimulator.h"
#include "HAL/IConsoleManager.h"
#include "HandMenuActor.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "VisionDisplayActor.h"

namespace
{
	TAutoConsoleVariable<float> CVarAutoCycleSeconds(
		TEXT("vision.AutoCycleSeconds"),
		0.0f,
		TEXT("Switch to the next vision condition every N seconds (0 = off). Read on BeginPlay."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSpawnDisplay(
		TEXT("vision.SpawnDisplay"),
		true,
		TEXT("Spawn the vision display actor (camera feed + image processing) on BeginPlay."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSpawnHandMenu(
		TEXT("vision.SpawnHandMenu"),
		true,
		TEXT("Spawn the hand menu on BeginPlay."),
		ECVF_Default);

	// Optional Blueprint subclass of AHandMenuActor (to tweak offsets in the editor) and the menu widget.
	const TCHAR* HandMenuActorPackage = TEXT("/Game/VisionSim/UI/BP_HandMenu");
	const TCHAR* HandMenuActorClassPath = TEXT("/Game/VisionSim/UI/BP_HandMenu.BP_HandMenu_C");
	const TCHAR* HandMenuWidgetPackage = TEXT("/Game/VisionSim/UI/WBP_HandMenu");
	const TCHAR* HandMenuWidgetClassPath = TEXT("/Game/VisionSim/UI/WBP_HandMenu.WBP_HandMenu_C");

	template <typename T>
	TSubclassOf<T> LoadOptionalClass(const TCHAR* PackageName, const TCHAR* ClassPath)
	{
		// Checking the package first avoids "failed to load" warnings when the asset was not created yet.
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			return nullptr;
		}
		return TSoftClassPtr<T>(FSoftObjectPath(ClassPath)).LoadSynchronous();
	}
}

bool UVisionSimulatorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVisionSimulatorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (CVarSpawnDisplay.GetValueOnGameThread())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("VisionDisplay");
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DisplayActor = InWorld.SpawnActor<AVisionDisplayActor>(SpawnParameters);
	}

	if (CVarSpawnHandMenu.GetValueOnGameThread())
	{
		SpawnHandMenu(InWorld);
	}

	const float AutoCycleSeconds = CVarAutoCycleSeconds.GetValueOnGameThread();
	if (AutoCycleSeconds > 0.0f)
	{
		InWorld.GetTimerManager().SetTimer(AutoCycleTimer, this, &UVisionSimulatorSubsystem::SelectNextCondition, AutoCycleSeconds, true);
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Vision condition auto-cycle every %.1f s"), AutoCycleSeconds);
	}
}

void UVisionSimulatorSubsystem::SpawnHandMenu(UWorld& InWorld)
{
	TSubclassOf<AHandMenuActor> ActorClass = LoadOptionalClass<AHandMenuActor>(HandMenuActorPackage, HandMenuActorClassPath);
	if (!ActorClass)
	{
		ActorClass = AHandMenuActor::StaticClass();
	}

	const FTransform SpawnTransform = FTransform::Identity;
	HandMenu = InWorld.SpawnActorDeferred<AHandMenuActor>(ActorClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!HandMenu)
	{
		return;
	}

	// The widget must be set before BeginPlay, when the ISDK widget component creates the widget instance.
	if (!HandMenu->GetMenuWidgetClass())
	{
		HandMenu->SetMenuWidgetClass(LoadOptionalClass<UUserWidget>(HandMenuWidgetPackage, HandMenuWidgetClassPath));
	}
	if (!HandMenu->GetMenuWidgetClass())
	{
		UE_LOG(LogEyeDiseasesSimulator, Warning, TEXT("Hand menu widget %s not found - the hand menu is empty"), HandMenuWidgetClassPath);
	}

	HandMenu->FinishSpawning(SpawnTransform);
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Hand menu spawned (%s)"), *ActorClass->GetName());
}

void UVisionSimulatorSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCycleTimer);
	}
	Super::Deinitialize();
}

void UVisionSimulatorSubsystem::SetCondition(EVisionCondition NewCondition)
{
	if (NewCondition == EVisionCondition::Count || NewCondition == Settings.Condition)
	{
		return;
	}
	Settings.Condition = NewCondition;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SetSeverity(float NewSeverity)
{
	NewSeverity = FMath::Clamp(NewSeverity, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(NewSeverity, Settings.Severity))
	{
		return;
	}
	Settings.Severity = NewSeverity;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SetRefractiveError(float Diopters)
{
	float* Target = nullptr;
	switch (Settings.Condition)
	{
	case EVisionCondition::Myopia:
		Target = &Settings.MyopiaDiopters;
		break;
	case EVisionCondition::Hyperopia:
		Target = &Settings.HyperopiaDiopters;
		break;
	default:
		return;
	}

	float Min = 0.0f;
	float Max = 0.0f;
	GetRefractiveErrorRange(Min, Max);
	Diopters = FMath::Clamp(Diopters, Min, Max);
	if (FMath::IsNearlyEqual(Diopters, *Target))
	{
		return;
	}
	*Target = Diopters;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SetAge(float NewAge)
{
	NewAge = FMath::Clamp(NewAge, 10.0f, 80.0f);
	if (FMath::IsNearlyEqual(NewAge, Settings.Age))
	{
		return;
	}
	Settings.Age = NewAge;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SetCompareMode(bool bEnabled)
{
	if (Settings.bCompareMode == bEnabled)
	{
		return;
	}
	Settings.bCompareMode = bEnabled;
	BroadcastChange();
}

float UVisionSimulatorSubsystem::GetRefractiveError() const
{
	return VisionOptics::RefractiveError(Settings);
}

void UVisionSimulatorSubsystem::GetRefractiveErrorRange(float& OutMin, float& OutMax) const
{
	switch (Settings.Condition)
	{
	case EVisionCondition::Myopia:
		OutMin = -MaxMyopiaDiopters;
		OutMax = -MinRefractiveErrorDiopters;
		break;
	case EVisionCondition::Hyperopia:
		OutMin = MinRefractiveErrorDiopters;
		OutMax = MaxHyperopiaDiopters;
		break;
	default:
		OutMin = 0.0f;
		OutMax = 0.0f;
		break;
	}
}

bool UVisionSimulatorSubsystem::GetSharpRange(float& OutNearPoint, float& OutFarPoint) const
{
	float MinVergence = 0.0f;
	float MaxVergence = 0.0f;
	VisionOptics::FocusRange(Settings, MinVergence, MaxVergence);

	// Vergence V = 1 / distance; V <= 0 lies at (or "beyond") infinity.
	OutFarPoint = MinVergence > 0.0f ? 1.0f / MinVergence : -1.0f;
	OutNearPoint = MaxVergence > 0.0f ? 1.0f / MaxVergence : -1.0f;
	return MaxVergence >= 0.0f;
}

void UVisionSimulatorSubsystem::SelectNextCondition()
{
	const int32 Next = (static_cast<int32>(Settings.Condition) + 1) % static_cast<int32>(EVisionCondition::Count);
	SetCondition(static_cast<EVisionCondition>(Next));
}

void UVisionSimulatorSubsystem::BroadcastChange()
{
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Vision: %s, severity %.2f, refractive error %+.2f D, age %.0f, compare %d"),
		*UEnum::GetValueAsString(Settings.Condition), Settings.Severity, GetRefractiveError(), Settings.Age,
		Settings.bCompareMode ? 1 : 0);
	OnSettingsChanged.Broadcast(Settings);
}
