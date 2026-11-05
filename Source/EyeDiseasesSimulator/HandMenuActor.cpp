#include "HandMenuActor.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "DataSources/IsdkIHandJoints.h"
#include "DataSources/IsdkIRootPose.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "IsdkHandMeshComponent.h"
#include "IsdkInteractableWidgetComponent.h"
#include "Rig/IsdkHandRigComponent.h"
#include "Rig/IsdkHandVisualsRigComponent.h"

namespace
{
	TAutoConsoleVariable<bool> CVarHandMenuDebug(
		TEXT("handmenu.Debug"),
		false,
		TEXT("Draw the palm normal and the hand menu detection state."),
		ECVF_Default);

	// Joint names of the ISDK hand mesh (UIsdkHandMeshComponent::SetMappedBoneNamesAsDefault).
	const FName WristBone(TEXT("XRHand_Wrist"));
	const FName PalmBone(TEXT("XRHand_Palm"));
	const FName IndexProximalBone(TEXT("XRHand_IndexProximal"));
	const FName MiddleProximalBone(TEXT("XRHand_MiddleProximal"));
	const FName LittleProximalBone(TEXT("XRHand_LittleProximal"));
}

AHandMenuActor::AHandMenuActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the pawn, the HMD camera and the ISDK hand meshes have been updated.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MenuWidgetComponent = CreateDefaultSubobject<UIsdkInteractableWidgetComponent>(TEXT("MenuWidget"));
	MenuWidgetComponent->SetupAttachment(Root);
}

void AHandMenuActor::SetMenuWidgetClass(TSubclassOf<UUserWidget> InClass)
{
	MenuWidgetClass = InClass;
}

UUserWidget* AHandMenuActor::GetMenuWidget() const
{
	const UWidgetComponent* Widget = MenuWidgetComponent ? MenuWidgetComponent->GetWidget() : nullptr;
	return Widget ? Widget->GetWidget() : nullptr;
}

void AHandMenuActor::BeginPlay()
{
	// The ISDK component creates the widget instance and the poke / ray surfaces in its BeginPlay.
	MenuWidgetComponent->WidgetClass = MenuWidgetClass;
	MenuWidgetComponent->DrawSize = DrawSize;
	MenuWidgetComponent->WidgetScale = WidgetScale;

	Super::BeginPlay();

	// Start hidden: invisible ISDK interactables are disabled, so hidden buttons cannot be pressed.
	bMenuVisible = false;
	Root->SetVisibility(false, /*bPropagateToChildren*/ true);
}

UIsdkHandMeshComponent* AHandMenuActor::FindHandMesh() const
{
	if (CachedHandMesh.IsValid())
	{
		return CachedHandMesh.Get();
	}

	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}

	TInlineComponentArray<UIsdkHandRigComponent*> HandRigs(Pawn);
	for (const UIsdkHandRigComponent* HandRig : HandRigs)
	{
		const bool bIsLeft = HandRig->IsA<UIsdkHandRigComponentLeft>();
		if (bIsLeft != bAttachToLeftHand)
		{
			continue;
		}
		if (const UIsdkHandVisualsRigComponent* Visuals = HandRig->GetHandVisuals())
		{
			// The synthetic hand is the one the user sees (the tracked one is hidden).
			UIsdkHandMeshComponent* Mesh = Visuals->SyntheticHandVisual ? Visuals->SyntheticHandVisual.Get() : Visuals->TrackedHandVisual.Get();
			CachedHandMesh = Mesh;
			return Mesh;
		}
	}
	return nullptr;
}

bool AHandMenuActor::GetHandFrame(FHandFrame& OutFrame) const
{
	UIsdkHandMeshComponent* Mesh = FindHandMesh();
	if (!Mesh)
	{
		return false;
	}

	// Tracking lost: the mesh keeps its last pose, so check the data sources.
	UObject* Joints = Mesh->GetJointsDataSource().GetObject();
	UObject* RootPose = Mesh->GetRootPoseDataSource().GetObject();
	if (!IsValid(Joints) || !IsValid(RootPose)
		|| !IIsdkIHandJoints::Execute_IsHandJointDataValid(Joints)
		|| !IIsdkIRootPose::Execute_IsRootPoseValid(RootPose))
	{
		return false;
	}

	const FVector Wrist = Mesh->GetBoneLocationByName(WristBone, EBoneSpaces::WorldSpace);
	const FVector Palm = Mesh->GetBoneLocationByName(PalmBone, EBoneSpaces::WorldSpace);
	const FVector IndexProximal = Mesh->GetBoneLocationByName(IndexProximalBone, EBoneSpaces::WorldSpace);
	const FVector MiddleProximal = Mesh->GetBoneLocationByName(MiddleProximalBone, EBoneSpaces::WorldSpace);
	const FVector LittleProximal = Mesh->GetBoneLocationByName(LittleProximalBone, EBoneSpaces::WorldSpace);

	// Palm plane spanned by wrist -> index and wrist -> little finger knuckles. For a left hand held up with the palm
	// toward the viewer, cross(Little - Wrist, Index - Wrist) points toward the viewer; the right hand is mirrored.
	const FVector ToIndex = IndexProximal - Wrist;
	const FVector ToLittle = LittleProximal - Wrist;
	const FVector Normal = bAttachToLeftHand ? FVector::CrossProduct(ToLittle, ToIndex) : FVector::CrossProduct(ToIndex, ToLittle);

	OutFrame.Palm = Palm;
	OutFrame.PalmNormal = Normal.GetSafeNormal();
	OutFrame.ThumbSide = (IndexProximal - LittleProximal).GetSafeNormal();
	OutFrame.FingerDir = (MiddleProximal - Wrist).GetSafeNormal();
	return !OutFrame.PalmNormal.IsNearlyZero();
}

bool AHandMenuActor::GetHeadPose(FVector& OutLocation, FVector& OutForward) const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager)
	{
		return false;
	}
	OutLocation = CameraManager->GetCameraLocation();
	OutForward = CameraManager->GetCameraRotation().Vector();
	return true;
}

FTransform AHandMenuActor::ComputeMenuTransform(const FHandFrame& Hand, const FVector& HeadLocation) const
{
	const FVector Location = Hand.Palm
		+ Hand.ThumbSide * ThumbSideOffset
		+ Hand.PalmNormal * PalmNormalOffset
		+ Hand.FingerDir * AlongFingersOffset;

	// The widget faces along its X axis: point X at the head and keep the menu upright.
	const FVector ToHead = (HeadLocation - Location).GetSafeNormal();
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(ToHead, FVector::UpVector).ToQuat() * WidgetRotationOffset.Quaternion();
	return FTransform(Rotation, Location);
}

void AHandMenuActor::SetMenuVisible(bool bVisible)
{
	if (bMenuVisible == bVisible)
	{
		return;
	}
	bMenuVisible = bVisible;
	HideTimer = 0.0f;

	// Propagates to the ISDK poke / ray interactables, which disable themselves while invisible.
	Root->SetVisibility(bVisible, /*bPropagateToChildren*/ true);

	if (bVisible)
	{
		OnMenuShown();
	}
	else
	{
		OnMenuHidden();
	}
}

void AHandMenuActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FHandFrame Hand;
	FVector HeadLocation;
	FVector HeadForward;
	if (!GetHandFrame(Hand) || !GetHeadPose(HeadLocation, HeadForward))
	{
		SetMenuVisible(false);
		return;
	}

	const FVector PalmToHead = (HeadLocation - Hand.Palm).GetSafeNormal();
	const float PalmFacing = FVector::DotProduct(Hand.PalmNormal, PalmToHead);
	const float Gaze = FVector::DotProduct(HeadForward, -PalmToHead);

	const bool bShowCondition = PalmFacing > ShowPalmFacing
		&& Gaze > FMath::Cos(FMath::DegreesToRadians(ShowGazeAngle));
	const bool bHideCondition = PalmFacing < HidePalmFacing
		|| Gaze < FMath::Cos(FMath::DegreesToRadians(HideGazeAngle));

	const FTransform Target = ComputeMenuTransform(Hand, HeadLocation);

	if (!bMenuVisible)
	{
		if (bShowCondition)
		{
			// Appear directly at the hand, without sliding in from the last position.
			SetActorTransform(Target);
			SetMenuVisible(true);
		}
	}
	else
	{
		HideTimer = bHideCondition ? HideTimer + DeltaSeconds : 0.0f;
		if (HideTimer >= HideDelay)
		{
			SetMenuVisible(false);
		}
		else if (FollowSpeed > 0.0f)
		{
			// Exponential smoothing reduces hand tracking jitter, which makes the buttons easier to hit.
			const float Alpha = 1.0f - FMath::Exp(-FollowSpeed * DeltaSeconds);
			SetActorLocationAndRotation(
				FMath::Lerp(GetActorLocation(), Target.GetLocation(), Alpha),
				FQuat::Slerp(GetActorQuat(), Target.GetRotation(), Alpha));
		}
		else
		{
			SetActorTransform(Target);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug || CVarHandMenuDebug.GetValueOnGameThread())
	{
		const FColor Color = bMenuVisible ? FColor::Green : (bShowCondition ? FColor::Yellow : FColor::Red);
		DrawDebugDirectionalArrow(GetWorld(), Hand.Palm, Hand.Palm + Hand.PalmNormal * 10.0f, 2.0f, Color, false, -1.0f, 0, 0.3f);
		DrawDebugLine(GetWorld(), Hand.Palm, Hand.Palm + Hand.ThumbSide * 5.0f, FColor::Blue, false, -1.0f, 0, 0.2f);
		DrawDebugString(GetWorld(), Hand.Palm, FString::Printf(TEXT("facing %.2f gaze %.2f"), PalmFacing, Gaze), nullptr, Color, 0.0f);
	}
#endif
}
