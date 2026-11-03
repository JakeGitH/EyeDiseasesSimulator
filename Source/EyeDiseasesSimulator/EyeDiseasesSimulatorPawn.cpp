#include "EyeDiseasesSimulatorPawn.h"

#include "EyeDiseasesSimulator.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "InputMappingContext.h"
#include "MotionControllerComponent.h"
#include "OculusXRPassthroughLayerComponent.h"
#include "OculusXRPassthroughLayerShapes.h"
#include "OculusXRPassthroughSubsystem.h"
#include "OculusXRPersistentPassthroughInstance.h"
#include "Rig/IsdkHandRigComponent.h"
#include "UObject/ConstructorHelpers.h"

AEyeDiseasesSimulatorPawn::AEyeDiseasesSimulatorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	Camera->bLockToHmd = true;

	MotionControllerLeft = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeft"));
	MotionControllerLeft->SetupAttachment(VROrigin);
	MotionControllerLeft->SetTrackingMotionSource(FName(TEXT("Left")));

	MotionControllerRight = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRight"));
	MotionControllerRight->SetupAttachment(VROrigin);
	MotionControllerRight->SetTrackingMotionSource(FName(TEXT("Right")));

	// ISDK hand rigs must be direct children of a motion controller to find their tracking data.
	LeftHandRig = CreateDefaultSubobject<UIsdkHandRigComponentLeft>(TEXT("LeftHandRig"));
	LeftHandRig->SetupAttachment(MotionControllerLeft);

	RightHandRig = CreateDefaultSubobject<UIsdkHandRigComponentRight>(TEXT("RightHandRig"));
	RightHandRig->SetupAttachment(MotionControllerRight);

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> HandImcFinder(
		TEXT("/OculusInteraction/Inputs/IMC_IsdkHand.IMC_IsdkHand"));
	if (HandImcFinder.Succeeded())
	{
		HandInputMappingContext = HandImcFinder.Object;
	}
}

void AEyeDiseasesSimulatorPawn::BeginPlay()
{
	// Bind before Super::BeginPlay, which begins play on the rigs.
	LeftHandRig->DataSourcesReady.AddDynamic(this, &AEyeDiseasesSimulatorPawn::HandleLeftHandDataSourcesReady);
	RightHandRig->DataSourcesReady.AddDynamic(this, &AEyeDiseasesSimulatorPawn::HandleRightHandDataSourcesReady);

	Super::BeginPlay();

	ApplyTrackingOrigin();

	if (bStartPassthrough)
	{
		StartPassthrough();
	}
}

void AEyeDiseasesSimulatorPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	AddHandInputMapping();
}

void AEyeDiseasesSimulatorPawn::ApplyTrackingOrigin() const
{
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		UE_LOG(LogEyeDiseasesSimulator, Warning, TEXT("No XR system - tracking origin not set (running without HMD?)"));
		return;
	}

	GEngine->XRSystem->SetTrackingOrigin(TrackingOrigin);
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Tracking origin set to %d"), static_cast<int32>(TrackingOrigin));
}

void AEyeDiseasesSimulatorPawn::AddHandInputMapping() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !HandInputMappingContext)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (!InputSubsystem)
	{
		UE_LOG(LogEyeDiseasesSimulator, Warning, TEXT("Enhanced Input subsystem missing - hand input mapping not added"));
		return;
	}

	InputSubsystem->AddMappingContext(HandInputMappingContext, HandInputMappingPriority);
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Hand input mapping context added: %s"), *HandInputMappingContext->GetName());
}

void AEyeDiseasesSimulatorPawn::StartPassthrough()
{
	UOculusXRPassthroughSubsystem* PassthroughSubsystem =
		UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!PassthroughSubsystem)
	{
		UE_LOG(LogEyeDiseasesSimulator, Warning,
			TEXT("Passthrough subsystem not available - check Project Settings > Meta XR > Passthrough Enabled"));
		return;
	}

	// Outer is the subsystem so the shape lives as long as the persistent layer, not this pawn.
	UOculusXRStereoLayerShapeReconstructed* Shape =
		NewObject<UOculusXRStereoLayerShapeReconstructed>(PassthroughSubsystem);
	Shape->LayerOrder = PassthroughLayerOrder_Underlay;
	Shape->TextureOpacityFactor = PassthroughOpacity;

	FOculusXRPersistentPassthroughParameters Parameters;
	Parameters.bVisible = true;
	Parameters.Shape = Shape;
	// Editor builds discard Shape and rebuild it from cached values, so they must be filled too (default is Overlay).
	Parameters.ApplyShape();

	FOculusXRPassthrough_LayerResumed_Single LayerResumed;
	LayerResumed.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AEyeDiseasesSimulatorPawn, HandlePassthroughLayerResumed));

	const UOculusXRPersistentPassthroughInstance* Instance =
		PassthroughSubsystem->InitializePersistentPassthrough(Parameters, LayerResumed);
	if (Instance)
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Persistent passthrough initialized (Underlay, opacity %.2f)"), PassthroughOpacity);
	}
	else
	{
		UE_LOG(LogEyeDiseasesSimulator, Error, TEXT("Persistent passthrough initialization failed"));
	}
}

void AEyeDiseasesSimulatorPawn::HandlePassthroughLayerResumed()
{
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Passthrough layer resumed (visible)"));
}

void AEyeDiseasesSimulatorPawn::HandleLeftHandDataSourcesReady()
{
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Left hand rig ready"));
}

void AEyeDiseasesSimulatorPawn::HandleRightHandDataSourcesReady()
{
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Right hand rig ready"));
}
