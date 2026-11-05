#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VisionDepthMap.generated.h"

class UTexture2D;

/**
 * Low-resolution depth map aligned with the passthrough camera image.
 *
 * The Quest depth texture is not exposed by the stock UE Meta XR plugin, so depth is sampled with
 * MRUK environment raycasts (Depth API): one ray per grid cell, from the camera through the cell center.
 * A fixed number of rays is cast per frame (round robin), so the whole grid refreshes every few frames.
 *
 * The texture stores vergence = 1 / distance [diopters] (R16F), because defocus is linear in vergence and
 * bilinear upsampling of vergence behaves better at depth edges than upsampling of distance.
 *
 * Console variables:
 *   vision.Depth.RaysPerFrame     - raycast budget per frame
 *   vision.Depth.FallbackDistance - distance (m) assumed where no depth is available (e.g. PC / no permission)
 */
UCLASS()
class EYEDISEASESSIMULATOR_API UVisionDepthMap : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(FIntPoint InGridSize);

	/** Starts / stops the MRUK environment raycaster (Depth API). Without it the map holds the fallback distance. */
	void SetActive(bool bInActive);
	bool IsActive() const { return bActive; }

	/**
	 * Casts the next batch of rays and uploads the grid to the texture.
	 * @param CameraToWorld          camera pose (X forward, Y right, Z up)
	 * @param TextureUVToCameraRay   camera-space ray direction through a point of the camera texture (UV 0-1)
	 */
	void Update(const FTransform& CameraToWorld, TFunctionRef<FVector(const FVector2D&)> TextureUVToCameraRay);

	UTexture2D* GetTexture() const { return Texture; }

	/** Vergences (diopters) of all grid cells, row-major. */
	const TArray<float>& GetVergences() const { return Vergences; }

	/** True once the raycaster is ready and returns hits; false = fallback values. */
	bool HasRealDepth() const { return bHasRealDepth; }

private:
	void FillFallback();
	void UploadTexture();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture;

	FIntPoint GridSize = FIntPoint::ZeroValue;
	TArray<float> Vergences;
	int32 NextCell = 0;
	bool bActive = false;
	bool bRaycasterCreated = false;
	bool bHasRealDepth = false;

	// Statistics for the periodic log line.
	double StatsStartTime = 0.0;
	double StatsRaycastSeconds = 0.0;
	int32 StatsRays = 0;
	int32 StatsHits = 0;
};
