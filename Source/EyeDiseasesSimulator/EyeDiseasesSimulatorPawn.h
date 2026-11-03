#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HeadMountedDisplayTypes.h"
#include "EyeDiseasesSimulatorPawn.generated.h"

class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UIsdkHandRigComponentLeft;
class UIsdkHandRigComponentRight;
class UInputMappingContext;

/**
 * Base MR pawn for Meta Quest 3 / 3S.
 * Layout: VROrigin -> Camera (follows the HMD), MotionControllerLeft/Right -> ISDK hand rigs (grab/poke/ray).
 * Persistent passthrough (underlay) starts in BeginPlay.
 * Do not add Oculus XR Hand components or a second passthrough layer in Blueprint - they would be duplicated.
 */
UCLASS()
class EYEDISEASESSIMULATOR_API AEyeDiseasesSimulatorPawn : public APawn
{
	GENERATED_BODY()

public:
	AEyeDiseasesSimulatorPawn();

	UFUNCTION(BlueprintPure, Category = "MR|Hands")
	UIsdkHandRigComponentLeft* GetLeftHandRig() const { return LeftHandRig; }

	UFUNCTION(BlueprintPure, Category = "MR|Hands")
	UIsdkHandRigComponentRight* GetRightHandRig() const { return RightHandRig; }

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;

	/** Origin of the player's tracking space (floor). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<USceneComponent> VROrigin;

	/** Camera locked to the headset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<UMotionControllerComponent> MotionControllerLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<UMotionControllerComponent> MotionControllerRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<UIsdkHandRigComponentLeft> LeftHandRig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MR|Components")
	TObjectPtr<UIsdkHandRigComponentRight> RightHandRig;

	/** LocalFloor keeps Z = 0 on the real floor, which MRUK room geometry expects. */
	UPROPERTY(EditDefaultsOnly, Category = "MR|Tracking")
	TEnumAsByte<EHMDTrackingOrigin::Type> TrackingOrigin = EHMDTrackingOrigin::LocalFloor;

	/** ISDK does not register its hand mapping context itself; without it pinch input actions never fire on Meta XR. */
	UPROPERTY(EditDefaultsOnly, Category = "MR|Input")
	TObjectPtr<UInputMappingContext> HandInputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "MR|Input")
	int32 HandInputMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "MR|Passthrough")
	bool bStartPassthrough = true;

	/** Passthrough texture opacity; below 1 dims the real room behind virtual content. */
	UPROPERTY(EditDefaultsOnly, Category = "MR|Passthrough", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PassthroughOpacity = 1.0f;

private:
	void ApplyTrackingOrigin() const;
	void AddHandInputMapping() const;
	void StartPassthrough();

	UFUNCTION()
	void HandlePassthroughLayerResumed();

	UFUNCTION()
	void HandleLeftHandDataSourcesReady();

	UFUNCTION()
	void HandleRightHandDataSourcesReady();
};
