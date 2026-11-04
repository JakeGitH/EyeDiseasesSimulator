#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MRUKPassthroughCameraAccess.h"
#include "VisionDisplayActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTextureRenderTarget2D;
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
 *   2. The render thread runs the VisionShaders passes: CameraTexture -> OutputTarget.
 *   3. OutputTarget is drawn on a quad that matches the camera frustum (built from the camera intrinsics),
 *      DisplayDistance in front of the camera.
 *
 * Spawned by UVisionSimulatorSubsystem; reads the current condition from it.
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

private:
	void RequestPermissionAndStart();
	void HandlePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults);
	void StartCamera();
	void StopCamera();

	void CreateOutputTarget(int32 Width, int32 Height);
	void BuildFeedMesh();
	void UpdateFeedTransform();
	void EnqueueImageProcessing(const FVisionEffectParams& Params);

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccess> CameraAccess;

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccessTexture> CameraTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OutputTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeedMaterialInstance;

	FDelegateHandle PermissionDelegateHandle;
	bool bCameraRunning = false;
	bool bFeedAttachedToHead = false;
};
