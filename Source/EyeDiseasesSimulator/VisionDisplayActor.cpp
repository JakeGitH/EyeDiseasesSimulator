#include "VisionDisplayActor.h"

#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
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
#include "VisionDepthMap.h"
#include "VisionImageProcessing.h"
#include "VisionSimulatorSubsystem.h"

namespace
{
	const TCHAR* HeadsetCameraPermission = TEXT("horizonos.permission.HEADSET_CAMERA");
	const TCHAR* ScenePermission = TEXT("com.oculus.permission.USE_SCENE");

	// Used only when the runtime reports no intrinsics (e.g. no headset). Roughly the Quest 3 camera FOV.
	constexpr double FallbackHorizontalFovDegrees = 80.0;

	TAutoConsoleVariable<float> CVarPupilDiameterMm(
		TEXT("vision.PupilDiameterMm"),
		5.0f,
		TEXT("Pupil diameter (mm) of the simulated eye. Blur circle = pupil * defocus; typical indoor value 4-6 mm."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDefocusScale(
		TEXT("vision.DefocusScale"),
		1.0f,
		TEXT("Multiplier of the physically based defocus blur (1 = no exaggeration)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugView(
		TEXT("vision.DebugView"),
		0,
		TEXT("0 = normal, 1 = vergence (depth) map, 2 = defocus blur sigma."),
		ECVF_Default);
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

bool AVisionDisplayActor::HasDepth() const
{
	return DepthMap && DepthMap->HasRealDepth();
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
	if (DepthMap)
	{
		DepthMap->SetActive(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AVisionDisplayActor::RequestPermissionAndStart()
{
#if PLATFORM_ANDROID
	const bool bCameraGranted = UAndroidPermissionFunctionLibrary::CheckPermission(HeadsetCameraPermission);
	bScenePermissionGranted = UAndroidPermissionFunctionLibrary::CheckPermission(ScenePermission);

	TArray<FString> Missing;
	if (!bCameraGranted)
	{
		Missing.Add(HeadsetCameraPermission);
	}
	if (!bScenePermissionGranted)
	{
		Missing.Add(ScenePermission);
	}

	if (Missing.Num() > 0)
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Requesting permissions: %s"), *FString::Join(Missing, TEXT(", ")));
		if (UAndroidPermissionCallbackProxy* Proxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(Missing))
		{
			PermissionDelegateHandle = Proxy->OnPermissionsGrantedDelegate.AddUObject(this, &AVisionDisplayActor::HandlePermissionsGranted);
		}
	}

	if (!bCameraGranted)
	{
		return;
	}
#else
	bScenePermissionGranted = true;
#endif
	StartCamera();
}

void AVisionDisplayActor::HandlePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
{
	// The proxy is a shared singleton and also reports permissions requested by other code.
	const int32 CameraIndex = Permissions.IndexOfByKey(FString(HeadsetCameraPermission));
	const int32 SceneIndex = Permissions.IndexOfByKey(FString(ScenePermission));
	if (CameraIndex == INDEX_NONE && SceneIndex == INDEX_NONE)
	{
		return;
	}

	UAndroidPermissionCallbackProxy::GetInstance()->OnPermissionsGrantedDelegate.Remove(PermissionDelegateHandle);
	PermissionDelegateHandle.Reset();

	if (SceneIndex != INDEX_NONE)
	{
		bScenePermissionGranted = GrantResults.IsValidIndex(SceneIndex) && GrantResults[SceneIndex];
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Permission %s %s"), ScenePermission,
			bScenePermissionGranted ? TEXT("granted") : TEXT("denied - depth-dependent blur uses a fixed distance"));
	}

	if (CameraIndex != INDEX_NONE)
	{
		if (GrantResults.IsValidIndex(CameraIndex) && GrantResults[CameraIndex])
		{
			UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Permission %s granted"), HeadsetCameraPermission);
			StartCamera();
		}
		else
		{
			UE_LOG(LogEyeDiseasesSimulator, Error, TEXT("Permission %s denied - camera image unavailable"), HeadsetCameraPermission);
		}
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

	DepthMap = NewObject<UVisionDepthMap>(this);
	DepthMap->Initialize(DepthGridSize);

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
		const double FallbackFocal = (Current.X * 0.5) / FMath::Tan(FMath::DegreesToRadians(FallbackHorizontalFovDegrees * 0.5));
		Focal = FVector2D(FallbackFocal, FallbackFocal);
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
	CropOrigin = Sensor * (FVector2D(1.0, 1.0) - Scale) * 0.5;
	CropSize = Sensor * Scale;
	FocalLength = Focal;
	PrincipalPoint = Principal;
	StreamFocalPixels = Focal.X * Current.X / CropSize.X;

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

FVector AVisionDisplayActor::TextureUVToCameraRay(const FVector2D& UV) const
{
	// Inverse of the UV flip applied to the quad in BuildFeedMesh (the flip is its own inverse).
	const double U = bFlipHorizontal ? 1.0 - UV.X : UV.X;
	const double V = bFlipVertical ? 1.0 - UV.Y : UV.Y;
	return FVector(
		1.0,
		(CropOrigin.X + CropSize.X * U - PrincipalPoint.X) / FocalLength.X,
		(CropOrigin.Y + CropSize.Y * V - PrincipalPoint.Y) / FocalLength.Y);
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
	const FVisionSettings Settings = Simulator ? Simulator->GetSettings() : FVisionSettings();
	FVisionEffectParams Params = VisionConditionPresets::MakeEffectParams(Settings);
	Params.CompareSplit = Settings.bCompareMode ? 0.5f : -1.0f;
	Params.DebugView = CVarDebugView.GetValueOnGameThread();

	UpdateDepth(Params);
	ApplyDefocusParams(Params);

	// On Android the camera texture is created with an sRGB format, so the GPU decodes it when sampling.
	// On PC (Link) the same bytes arrive in a linear texture and must be decoded in the shader.
	Params.bDecodeInputSRGB = !PLATFORM_ANDROID;

	EnqueueImageProcessing(Params);
}

void AVisionDisplayActor::UpdateDepth(const FVisionEffectParams& Params)
{
	if (!DepthMap)
	{
		return;
	}

	// The Depth API costs GPU time and power: run it only while a depth-based condition (or a debug view) is shown.
	const bool bNeedsDepth = bScenePermissionGranted && (Params.bDepthDefocus || Params.DebugView != 0);
	DepthMap->SetActive(bNeedsDepth);
	if (bNeedsDepth)
	{
		// The quad sits at the camera pose (head-locked: head + lens offset, world-locked: frame pose).
		const FTransform CameraToWorld(FeedMesh->GetComponentQuat(), FeedMesh->GetComponentLocation());
		DepthMap->Update(CameraToWorld, [this](const FVector2D& UV) { return TextureUVToCameraRay(UV); });
	}
}

void AVisionDisplayActor::ApplyDefocusParams(FVisionEffectParams& Params) const
{
	if (!Params.bDepthDefocus || !DepthMap)
	{
		return;
	}

	// Geometric optics: an object with defocus D (diopters) forms a blur circle of angular diameter pupil[m] * D [rad].
	// A uniform disk of radius r has a per-axis standard deviation of r / 2, so the matching Gaussian has
	// sigma = pupil * D / 4 [rad]; times the focal length in pixels gives sigma in image pixels.
	const double PupilMeters = FMath::Max(CVarPupilDiameterMm.GetValueOnGameThread(), 0.5f) * 1.0e-3;
	Params.BlurSigmaPerDiopter = static_cast<float>(StreamFocalPixels * PupilMeters / 4.0 * CVarDefocusScale.GetValueOnGameThread());

	// Strongest defocus in the current depth map sets the blur levels computed on the GPU.
	float MaxDefocus = 0.0f;
	for (const float Vergence : DepthMap->GetVergences())
	{
		MaxDefocus = FMath::Max(MaxDefocus, VisionOptics::Defocus(Vergence, Params.FocusVergenceMin, Params.FocusVergenceMax));
	}
	Params.MaxDefocusDiopters = MaxDefocus;
}

void AVisionDisplayActor::EnqueueImageProcessing(const FVisionEffectParams& Params)
{
	FTextureResource* InputResource = CameraTexture->GetResource();
	FTextureRenderTargetResource* OutputResource = OutputTarget->GameThread_GetRenderTargetResource();
	if (!InputResource || !OutputResource)
	{
		return;
	}

	const bool bUseVergence = Params.bDepthDefocus || Params.DebugView != 0;
	FTextureResource* VergenceResource = (bUseVergence && DepthMap && DepthMap->GetTexture())
		? DepthMap->GetTexture()->GetResource()
		: nullptr;

	ENQUEUE_RENDER_COMMAND(VisionImageProcessing)(
		[InputResource, OutputResource, VergenceResource, Params](FRHICommandListImmediate& RHICmdList)
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

			FRDGTextureRef Vergence = nullptr;
			if (VergenceResource && VergenceResource->TextureRHI)
			{
				Vergence = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(VergenceResource->TextureRHI, TEXT("VisionVergenceMap")));
			}

			VisionImageProcessing::AddVisionEffectPasses(GraphBuilder, Input, Output, Params, Vergence);

			// The feed material samples the result later in the frame.
			GraphBuilder.SetTextureAccessFinal(Output, ERHIAccess::SRVMask);
			GraphBuilder.Execute();
		});
}
