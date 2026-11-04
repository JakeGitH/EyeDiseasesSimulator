#include "VisionDisplayActor.h"

#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EyeDiseasesSimulator.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RenderGraphBuilder.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "VisionCondition.h"
#include "VisionImageProcessing.h"
#include "VisionSimulatorSubsystem.h"

namespace
{
	const TCHAR* HeadsetCameraPermission = TEXT("horizonos.permission.HEADSET_CAMERA");

	// Used only when the runtime reports no intrinsics (e.g. no headset). Roughly the Quest 3 camera FOV.
	constexpr double FallbackHorizontalFovDegrees = 80.0;
}

AVisionDisplayActor::AVisionDisplayActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the pawn and its HMD camera have been updated for this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	FeedMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FeedMesh"));
	FeedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeedMesh->SetCastShadow(false);
	FeedMesh->SetVisibility(false);
	SetRootComponent(FeedMesh);

	FeedMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VisionSim/Materials/M_VisionFeed.M_VisionFeed")));
}

UTexture* AVisionDisplayActor::GetCameraTexture() const
{
	return CameraTexture;
}

void AVisionDisplayActor::BeginPlay()
{
	Super::BeginPlay();
	RequestPermissionAndStart();
}

void AVisionDisplayActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PermissionDelegateHandle.IsValid())
	{
		UAndroidPermissionCallbackProxy::GetInstance()->OnPermissionsGrantedDelegate.Remove(PermissionDelegateHandle);
		PermissionDelegateHandle.Reset();
	}
	StopCamera();
	Super::EndPlay(EndPlayReason);
}

void AVisionDisplayActor::RequestPermissionAndStart()
{
#if PLATFORM_ANDROID
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(HeadsetCameraPermission))
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Requesting permission %s"), HeadsetCameraPermission);
		TArray<FString> Permissions;
		Permissions.Add(HeadsetCameraPermission);
		if (UAndroidPermissionCallbackProxy* Proxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions))
		{
			PermissionDelegateHandle = Proxy->OnPermissionsGrantedDelegate.AddUObject(this, &AVisionDisplayActor::HandlePermissionsGranted);
		}
		return;
	}
#endif
	StartCamera();
}

void AVisionDisplayActor::HandlePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
{
	// The proxy is a shared singleton and also reports permissions requested by other code.
	const int32 Index = Permissions.IndexOfByKey(FString(HeadsetCameraPermission));
	if (Index == INDEX_NONE)
	{
		return;
	}

	UAndroidPermissionCallbackProxy::GetInstance()->OnPermissionsGrantedDelegate.Remove(PermissionDelegateHandle);
	PermissionDelegateHandle.Reset();

	if (GrantResults.IsValidIndex(Index) && GrantResults[Index])
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Permission %s granted"), HeadsetCameraPermission);
		StartCamera();
	}
	else
	{
		UE_LOG(LogEyeDiseasesSimulator, Error, TEXT("Permission %s denied - camera image unavailable"), HeadsetCameraPermission);
	}
}

void AVisionDisplayActor::StartCamera()
{
	CameraAccess = NewObject<UMRUKPassthroughCameraAccess>(this);
	CameraAccess->CameraEye = CameraEye;
	CameraAccess->ResolutionWidth = RequestedResolution.X;
	CameraAccess->ResolutionHeight = RequestedResolution.Y;
	CameraAccess->MaxFramerate = MaxFramerate;

	CameraTexture = NewObject<UMRUKPassthroughCameraAccessTexture>(this);
	CameraTexture->PassthroughCameraAccess = CameraAccess;
	CameraTexture->UpdateResource();
	// The texture registers itself with the PCA subsystem in InitRHI (render thread); Play() needs it registered.
	FlushRenderingCommands();

	if (!CameraAccess->Play())
	{
		UE_LOG(LogEyeDiseasesSimulator, Error,
			TEXT("Passthrough camera failed to start (requires Quest 3/3S, Vulkan and the HEADSET_CAMERA permission)"));
		CameraTexture->UnregisterTexture();
		return;
	}

	bCameraRunning = true;
	UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Passthrough camera %s started at %dx%d"),
		*UEnum::GetValueAsString(CameraEye), CameraAccess->ResolutionWidth, CameraAccess->ResolutionHeight);

	CreateOutputTarget(CameraAccess->ResolutionWidth, CameraAccess->ResolutionHeight);
	BuildFeedMesh();

	if (UMaterialInterface* Material = FeedMaterial.LoadSynchronous())
	{
		FeedMaterialInstance = UMaterialInstanceDynamic::Create(Material, this);
		FeedMaterialInstance->SetTextureParameterValue(FeedTextureParameter, OutputTarget);
		FeedMesh->SetMaterial(0, FeedMaterialInstance);
	}
	else
	{
		UE_LOG(LogEyeDiseasesSimulator, Error, TEXT("Feed material %s not found - run Tools/CreateVisionAssets.py"),
			*FeedMaterial.ToString());
	}

	FeedMesh->SetVisibility(true);
}

void AVisionDisplayActor::StopCamera()
{
	if (CameraAccess && CameraAccess->IsCameraPlaying())
	{
		CameraAccess->Stop();
	}
	if (CameraTexture)
	{
		CameraTexture->UnregisterTexture();
	}
	bCameraRunning = false;
}

void AVisionDisplayActor::CreateOutputTarget(int32 Width, int32 Height)
{
	OutputTarget = NewObject<UTextureRenderTarget2D>(this);
	OutputTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	OutputTarget->ClearColor = FLinearColor::Black;
	OutputTarget->bAutoGenerateMips = false;
	OutputTarget->InitAutoFormat(Width, Height);
	OutputTarget->UpdateResourceImmediate(true);
}

void AVisionDisplayActor::BuildFeedMesh()
{
	const FMRUKCameraIntrinsics Intrinsics = CameraAccess->GetCameraIntrinsics();
	const FVector2D Current(CameraAccess->ResolutionWidth, CameraAccess->ResolutionHeight);

	FVector2D Focal = Intrinsics.FocalLength;
	FVector2D Principal = Intrinsics.PrincipalPoint;
	FVector2D Sensor(Intrinsics.SensorResolution.X, Intrinsics.SensorResolution.Y);
	if (Focal.X <= 0.0 || Focal.Y <= 0.0 || Sensor.X <= 0.0 || Sensor.Y <= 0.0)
	{
		UE_LOG(LogEyeDiseasesSimulator, Warning, TEXT("No camera intrinsics - using a %.0f deg fallback FOV"), FallbackHorizontalFovDegrees);
		Sensor = Current;
		Principal = Current * 0.5;
		const double FocalLength = (Current.X * 0.5) / FMath::Tan(FMath::DegreesToRadians(FallbackHorizontalFovDegrees * 0.5));
		Focal = FVector2D(FocalLength, FocalLength);
	}
	else
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Camera intrinsics: focal (%.1f, %.1f), principal (%.1f, %.1f), sensor %.0fx%.0f, lens offset %s / %s"),
			Focal.X, Focal.Y, Principal.X, Principal.Y, Sensor.X, Sensor.Y,
			*Intrinsics.LensOffsetPosition.ToString(), *Intrinsics.LensOffsetOrientation.Rotator().ToString());
	}

	// The streamed image is a centered crop of the sensor with the stream's aspect ratio
	// (same as UMRUKPassthroughCameraAccessSubsystem::CalcSensorCropRegion).
	FVector2D Scale = Current / Sensor;
	Scale /= Scale.GetMax();
	const FVector2D CropOrigin = Sensor * (FVector2D(1.0, 1.0) - Scale) * 0.5;
	const FVector2D CropSize = Sensor * Scale;

	// Pinhole model: viewport point (U, V) -> ray (1, (x - cx) / fx, (y - cy) / fy) in camera space (X forward, Y right, Z up),
	// matching UMRUKPassthroughCameraAccessSubsystem::ViewportPointToWorldSpaceRay. The quad lies on the plane X = DisplayDistance.
	auto MakeCorner = [&](double U, double V)
	{
		const double Y = (CropOrigin.X + CropSize.X * U - Principal.X) / Focal.X;
		const double Z = (CropOrigin.Y + CropSize.Y * V - Principal.Y) / Focal.Y;
		return FVector(1.0, Y, Z) * DisplayDistance;
	};

	auto MakeUV = [&](double U, double V)
	{
		return FVector2D(bFlipHorizontal ? 1.0 - U : U, bFlipVertical ? 1.0 - V : V);
	};

	const TArray<FVector> Vertices = { MakeCorner(0, 0), MakeCorner(1, 0), MakeCorner(0, 1), MakeCorner(1, 1) };
	const TArray<FVector2D> UVs = { MakeUV(0, 0), MakeUV(1, 0), MakeUV(0, 1), MakeUV(1, 1) };
	const TArray<int32> Triangles = { 0, 1, 2, 2, 1, 3 };
	const TArray<FVector> Normals = { FVector::BackwardVector, FVector::BackwardVector, FVector::BackwardVector, FVector::BackwardVector };
	const TArray<FLinearColor> Colors = { FLinearColor::White, FLinearColor::White, FLinearColor::White, FLinearColor::White };
	const TArray<FProcMeshTangent> Tangents = { FProcMeshTangent(0, 1, 0), FProcMeshTangent(0, 1, 0), FProcMeshTangent(0, 1, 0), FProcMeshTangent(0, 1, 0) };

	FeedMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, /*bCreateCollision*/ false);
}

void AVisionDisplayActor::UpdateFeedTransform()
{
	if (Placement == EVisionFeedPlacement::HeadLocked)
	{
		if (bFeedAttachedToHead)
		{
			return;
		}

		const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		UCameraComponent* HeadCamera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!HeadCamera)
		{
			return;
		}

		const FMRUKCameraIntrinsics Intrinsics = CameraAccess->GetCameraIntrinsics();
		FeedMesh->AttachToComponent(HeadCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FeedMesh->SetRelativeTransform(FTransform(Intrinsics.LensOffsetOrientation, Intrinsics.LensOffsetPosition));
		bFeedAttachedToHead = true;
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Camera feed attached to %s"), *HeadCamera->GetPathName());
		return;
	}

	// World-locked: camera pose at the frame timestamp (tracking space) -> world.
	FVector CameraPosition;
	FQuat CameraOrientation;
	CameraAccess->GetCameraPose(CameraPosition, CameraOrientation);

	const FTransform TrackingToWorld = (GEngine && GEngine->XRSystem.IsValid())
		? GEngine->XRSystem->GetTrackingToWorldTransform()
		: FTransform::Identity;
	FeedMesh->SetWorldTransform(FTransform(CameraOrientation, CameraPosition) * TrackingToWorld);
}

void AVisionDisplayActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bCameraRunning || !CameraTexture || !OutputTarget)
	{
		return;
	}

	UpdateFeedTransform();

	const UVisionSimulatorSubsystem* Simulator = GetWorld()->GetSubsystem<UVisionSimulatorSubsystem>();
	FVisionEffectParams Params = Simulator
		? VisionConditionPresets::MakeEffectParams(Simulator->GetCondition(), Simulator->GetSeverity())
		: FVisionEffectParams();

	// On Android the camera texture is created with an sRGB format, so the GPU decodes it when sampling.
	// On PC (Link) the same bytes arrive in a linear texture and must be decoded in the shader.
	Params.bDecodeInputSRGB = !PLATFORM_ANDROID;

	EnqueueImageProcessing(Params);
}

void AVisionDisplayActor::EnqueueImageProcessing(const FVisionEffectParams& Params)
{
	FTextureResource* InputResource = CameraTexture->GetResource();
	FTextureRenderTargetResource* OutputResource = OutputTarget->GameThread_GetRenderTargetResource();
	if (!InputResource || !OutputResource)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(VisionImageProcessing)(
		[InputResource, OutputResource, Params](FRHICommandListImmediate& RHICmdList)
		{
			// Read on the render thread: the PCA texture recreates its RHI texture when the resolution changes.
			FRHITexture* InputRHI = InputResource->TextureRHI;
			FRHITexture* OutputRHI = OutputResource->GetRenderTargetTexture();
			if (!InputRHI || !OutputRHI)
			{
				return;
			}

			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("VisionSimulator"));
			FRDGTextureRef Input = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InputRHI, TEXT("VisionCameraFrame")));
			FRDGTextureRef Output = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputRHI, TEXT("VisionOutput")));

			VisionImageProcessing::AddVisionEffectPasses(GraphBuilder, Input, Output, Params);

			// The feed material samples the result later in the frame.
			GraphBuilder.SetTextureAccessFinal(Output, ERHIAccess::SRVMask);
			GraphBuilder.Execute();
		});
}
