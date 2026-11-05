#include "VisionDepthMap.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EyeDiseasesSimulator.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "IXRTrackingSystem.h"
#include "MRUtilityKitSubsystem.h"
#include "TextureResource.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDepthRaysPerFrame(
		TEXT("vision.Depth.RaysPerFrame"),
		300,
		TEXT("Number of depth raycasts per frame for the defocus depth map (round robin over the grid)."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarDepthFallbackDistance(
		TEXT("vision.Depth.FallbackDistance"),
		2.0f,
		TEXT("Distance in meters assumed where no depth is available (no Depth API, permission denied, PC)."),
		ECVF_Default);

	// Vergence range stored in the map: 0 D = infinity, 10 D = 10 cm.
	constexpr float MaxVergence = 10.0f;

	constexpr double StatsLogIntervalSeconds = 10.0;

	UMRUKSubsystem* GetMRUK(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UMRUKSubsystem>() : nullptr;
	}
}

void UVisionDepthMap::Initialize(FIntPoint InGridSize)
{
	GridSize = FIntPoint(FMath::Max(InGridSize.X, 1), FMath::Max(InGridSize.Y, 1));
	Vergences.SetNumZeroed(GridSize.X * GridSize.Y);

	// R16F is filterable on every Vulkan device, so the shader gets bilinear upsampling for free.
	Texture = UTexture2D::CreateTransient(GridSize.X, GridSize.Y, PF_R16F, TEXT("VisionVergenceMap"));
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->UpdateResource();

	FillFallback();
	UploadTexture();
}

void UVisionDepthMap::SetActive(bool bInActive)
{
	if (bActive == bInActive)
	{
		return;
	}
	bActive = bInActive;

	UMRUKSubsystem* MRUK = GetMRUK(this);
	if (bActive)
	{
		if (MRUK)
		{
			MRUK->CreateEnvironmentRaycaster();
			bRaycasterCreated = true;
			UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Depth map: environment raycaster requested (%dx%d grid)"), GridSize.X, GridSize.Y);
		}
		StatsStartTime = FPlatformTime::Seconds();
	}
	else
	{
		if (MRUK && bRaycasterCreated)
		{
			MRUK->DestroyEnvironmentRaycaster();
		}
		bRaycasterCreated = false;
		bHasRealDepth = false;
		FillFallback();
		UploadTexture();
	}
}

void UVisionDepthMap::FillFallback()
{
	const float FallbackDistance = FMath::Max(CVarDepthFallbackDistance.GetValueOnGameThread(), 0.1f);
	for (float& Vergence : Vergences)
	{
		Vergence = 1.0f / FallbackDistance;
	}
}

void UVisionDepthMap::Update(const FTransform& CameraToWorld, TFunctionRef<FVector(const FVector2D&)> TextureUVToCameraRay)
{
	if (!bActive || !Texture)
	{
		return;
	}

	// RaycastEnvironment dereferences the XR system without a check.
	UMRUKSubsystem* MRUK = GetMRUK(this);
	if (!MRUK || !GEngine || !GEngine->XRSystem.IsValid()
		|| MRUK->EnvironmentRaycasterStatus() != EMRUKEnvironmentRaycasterStatus::Ready)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float WorldToMeters = (World && World->GetWorldSettings()) ? World->GetWorldSettings()->WorldToMeters : 100.0f;
	const FVector Origin = CameraToWorld.GetLocation();
	const int32 NumCells = Vergences.Num();
	const int32 NumRays = FMath::Clamp(CVarDepthRaysPerFrame.GetValueOnGameThread(), 1, NumCells);

	const double StartTime = FPlatformTime::Seconds();
	for (int32 Ray = 0; Ray < NumRays; ++Ray)
	{
		const int32 Cell = NextCell;
		NextCell = (NextCell + 1) % NumCells;

		// Ray through the cell center, in the UV space of the camera texture.
		const FVector2D UV((Cell % GridSize.X + 0.5) / GridSize.X, (Cell / GridSize.X + 0.5) / GridSize.Y);
		const FVector Direction = CameraToWorld.TransformVectorNoScale(TextureUVToCameraRay(UV)).GetSafeNormal();

		const FMRUKEnvironmentRaycastHit Hit = MRUK->RaycastEnvironment(Origin, Direction);
		switch (Hit.status)
		{
		case EMRUKEnvironmentRaycastHitStatus::Hit:
		{
			const float DistanceMeters = FVector::Distance(Origin, Hit.point) / WorldToMeters;
			Vergences[Cell] = FMath::Min(1.0f / FMath::Max(DistanceMeters, 1e-3f), MaxVergence);
			bHasRealDepth = true;
			++StatsHits;
			break;
		}
		case EMRUKEnvironmentRaycastHitStatus::NoHit:
			// Nothing within the depth range: treat as far away.
			Vergences[Cell] = 0.0f;
			break;
		default:
			// Occluded / outside the depth sensor FOV / failure: keep the previous value.
			break;
		}
	}
	StatsRaycastSeconds += FPlatformTime::Seconds() - StartTime;
	StatsRays += NumRays;

	UploadTexture();

	const double Now = FPlatformTime::Seconds();
	if (Now - StatsStartTime > StatsLogIntervalSeconds && StatsRays > 0)
	{
		UE_LOG(LogEyeDiseasesSimulator, Log, TEXT("Depth map: %d rays, %.1f%% hits, %.3f ms per frame (%.2f us per ray)"),
			StatsRays, 100.0 * StatsHits / StatsRays,
			1000.0 * StatsRaycastSeconds * NumRays / StatsRays,
			1.0e6 * StatsRaycastSeconds / StatsRays);
		StatsStartTime = Now;
		StatsRaycastSeconds = 0.0;
		StatsRays = 0;
		StatsHits = 0;
	}
}

void UVisionDepthMap::UploadTexture()
{
	if (!Texture)
	{
		return;
	}

	const int32 NumCells = Vergences.Num();
	FFloat16* Data = new FFloat16[NumCells];
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		Data[Cell] = FFloat16(Vergences[Cell]);
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, GridSize.X, GridSize.Y);
	Texture->UpdateTextureRegions(0, 1, Region, GridSize.X * sizeof(FFloat16), sizeof(FFloat16), reinterpret_cast<uint8*>(Data),
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] reinterpret_cast<FFloat16*>(SrcData);
			delete Regions;
		});
}
