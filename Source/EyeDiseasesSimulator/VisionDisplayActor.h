#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MRUKPassthroughCameraAccess.h"
#include "VisionDisplayActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTextureRenderTarget2D;
class UVisionDepthMap;
struct FVisionEffectParams;

UENUM(BlueprintType)
enum class EVisionFeedPlacement : uint8
{
	/** Quad follows the headset with the camera's fixed offset. Robust, but the image lags slightly when turning the head. */
	HeadLocked,
	/** Quad is placed at the camera pose of the frame timestamp, so the image stays aligned with the room. */
	WorldLocked
};

/**
 * Shows the Quest passthrough camera image after vision impairment processing.
 *
 * Every frame:
 *   1. MRUK Passthrough Camera Access updates CameraTexture with the newest camera frame.
 *   2. For refractive errors, UVisionDepthMap samples the scene depth along camera rays (vergence map).
 *   3. The render thread runs the VisionShaders passes: CameraTexture (+ vergence map) -> OutputTarget.
 *   4. OutputTarget is drawn on a quad that matches the camera frustum (built from the camera intrinsics),
 *      DisplayDistance in front of the camera.
 *
 * Spawned by UVisionSimulatorSubsystem; reads the current settings from it.
 *
 * Console variables:
 *   vision.PupilDiameterMm - pupil diameter of the simulated eye; scales the defocus blur
 *   vision.DefocusScale    - extra multiplier of the defocus blur (1 = physically based)
 *   vision.DebugView       - 0 normal, 1 vergence (depth) map, 2 defocus blur sigma
 */
UCLASS()
class EYEDISEASESSIMULATOR_API AVisionDisplayActor : public AActor
{
	GENERATED_BODY()

public:
	AVisionDisplayActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	bool IsCameraRunning() const { return bCameraRunning; }

	/** Unprocessed camera frame. */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	UTexture* GetCameraTexture() const;

	/** Processed frame (what the user sees). */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	UTextureRenderTarget2D* GetProcessedTexture() const { return OutputTarget; }

	/** True when the depth map holds real Depth API data (not the fallback distance). */
	UFUNCTION(BlueprintPure, Category = "Vision Simulator")
	bool HasDepth() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Vision Simulator")
	TObjectPtr<UProceduralMeshComponent> FeedMesh;

	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Camera")
	EMRUKCameraEye CameraEye = EMRUKCameraEye::Left;

	/** Requested camera resolution; the closest supported one is used. */
	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Camera")
	FIntPoint RequestedResolution = FIntPoint(1280, 960);

	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Camera", meta = (ClampMin = "1", ClampMax = "60"))
	int32 MaxFramerate = 60;

	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display")
	EVisionFeedPlacement Placement = EVisionFeedPlacement::HeadLocked;

	/** Distance of the image quad from the camera (cm). Objects at this distance line up best with reality. */
	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display", meta = (ClampMin = "20.0"))
	float DisplayDistance = 200.0f;

	/** Flip the image vertically / horizontally if it appears upside down or mirrored on the device. */
	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display")
	bool bFlipVertical = true;

	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display")
	bool bFlipHorizontal = false;

	/** Unlit material with a texture parameter (FeedTextureParameter) that shows the processed image. */
	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display")
	TSoftObjectPtr<UMaterialInterface> FeedMaterial;

	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Display")
	FName FeedTextureParameter = TEXT("FeedTexture");

	/** Resolution of the depth (vergence) map used for depth-dependent defocus. Rays per frame: vision.Depth.RaysPerFrame. */
	UPROPERTY(EditAnywhere, Category = "Vision Simulator|Depth")
	FIntPoint DepthGridSize = FIntPoint(40, 30);

private:
	void RequestPermissionAndStart();
	void HandlePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults);
	void StartCamera();
	void StopCamera();

	void CreateOutputTarget(int32 Width, int32 Height);
	void BuildFeedMesh();
	void UpdateFeedTransform();
	void UpdateDepth(const FVisionEffectParams& Params);
	void ApplyDefocusParams(FVisionEffectParams& Params) const;
	void EnqueueImageProcessing(const FVisionEffectParams& Params);

	/** Camera-space ray (X forward, Y right, Z up) through a point of the camera texture (UV 0-1), pinhole model. */
	FVector TextureUVToCameraRay(const FVector2D& UV) const;

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccess> CameraAccess;

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccessTexture> CameraTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OutputTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeedMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UVisionDepthMap> DepthMap;

	FDelegateHandle PermissionDelegateHandle;
	bool bCameraRunning = false;
	bool bFeedAttachedToHead = false;

	/** com.oculus.permission.USE_SCENE (spatial data) is required by the Depth API. */
	bool bScenePermissionGranted = false;

	// Pinhole camera model of the streamed image (pixels of the sensor), set in BuildFeedMesh.
	FVector2D FocalLength = FVector2D::UnitVector;
	FVector2D PrincipalPoint = FVector2D::ZeroVector;
	FVector2D CropOrigin = FVector2D::ZeroVector;
	FVector2D CropSize = FVector2D::UnitVector;

	/** Focal length in pixels of the streamed (processed) image: converts view angles to pixels. */
	double StreamFocalPixels = 1.0;
};
