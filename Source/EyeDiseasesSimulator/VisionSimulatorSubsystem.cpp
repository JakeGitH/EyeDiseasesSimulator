#include "VisionSimulatorSubsystem.h"

#include "EyeDiseasesSimulator.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "VisionDisplayActor.h"

namespace
{
	TAutoConsoleVariable<float> CVarAutoCycleSeconds(
		TEXT("vision.AutoCycleSeconds"),
		6.0f,
		TEXT("Switch to the next vision condition every N seconds (0 = off). Read on BeginPlay."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSpawnDisplay(
		TEXT("vision.SpawnDisplay"),
		true,
		TEXT("Spawn the vision display actor (camera feed + image processing) on BeginPlay."),
		ECVF_Default);
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

	const float AutoCycleSeconds = CVarAutoCycleSeconds.GetValueOnGameThread();
	if (AutoCycleSeconds > 0.0f)
	{
		InWorld.GetTimerManager().SetTimer(AutoCycleTimer, this, &UVisionSimulatorSubsystem::SelectNextCondition, AutoCycleSeconds, true);
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Vision condition auto-cycle every %.1f s"), AutoCycleSeconds);
	}
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
	if (NewCondition == EVisionCondition::Count || NewCondition == Condition)
	{
		return;
	}
	Condition = NewCondition;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SetSeverity(float NewSeverity)
{
	NewSeverity = FMath::Clamp(NewSeverity, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(NewSeverity, Severity))
	{
		return;
	}
	Severity = NewSeverity;
	BroadcastChange();
}

void UVisionSimulatorSubsystem::SelectNextCondition()
{
	const int32 Next = (static_cast<int32>(Condition) + 1) % static_cast<int32>(EVisionCondition::Count);
	SetCondition(static_cast<EVisionCondition>(Next));
}

void UVisionSimulatorSubsystem::BroadcastChange()
{
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Vision condition: %s, severity %.2f"),
		*UEnum::GetValueAsString(Condition), Severity);
	OnConditionChanged.Broadcast(Condition, Severity);
}
