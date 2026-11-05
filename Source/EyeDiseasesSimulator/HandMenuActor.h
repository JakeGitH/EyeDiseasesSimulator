#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HandMenuActor.generated.h"

class UIsdkHandMeshComponent;
class UIsdkInteractableWidgetComponent;
class USceneComponent;
class UUserWidget;

/**
 * Wrist menu: a UMG widget that appears next to one hand when its palm faces the user's face
 * and the user looks at it. Buttons are pressed by poking with the other hand's index finger
 * (ISDK poke interactor of the hand rig) or with the ISDK ray.
 *
 * Detection (every frame, hand joints of the ISDK hand mesh):
 *   palm normal  n = normalize(cross(LittleProximal - Wrist, IndexProximal - Wrist))   (sign flipped for the right hand)
 *   palm facing  f = dot(n, normalize(Head - Palm))       - palm turned toward the face
 *   gaze         g = dot(HeadForward, normalize(Palm - Head)) - user looks at the hand
 *   show when f > ShowPalmFacing and g > cos(ShowGazeAngle); hide when f < HidePalmFacing or g < cos(HideGazeAngle)
 *   for longer than HideDelay (hysteresis, so the menu does not flicker at the threshold).
 *
 * Generic: knows nothing about the vision simulator, can be moved back to the MR template.
 * Finds the hand through the ISDK hand rig of the player pawn.
 */
UCLASS(Blueprintable)
class EYEDISEASESSIMULATOR_API AHandMenuActor : public AActor
{
	GENERATED_BODY()

public:
	AHandMenuActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Hand Menu")
	bool IsMenuVisible() const { return bMenuVisible; }

	/** Instance of MenuWidgetClass created by the ISDK widget component. */
	UFUNCTION(BlueprintPure, Category = "Hand Menu")
	UUserWidget* GetMenuWidget() const;

	TSubclassOf<UUserWidget> GetMenuWidgetClass() const { return MenuWidgetClass; }

	/** Must be called before BeginPlay (e.g. between SpawnActorDeferred and FinishSpawning). */
	void SetMenuWidgetClass(TSubclassOf<UUserWidget> InClass);

protected:
	virtual void BeginPlay() override;

	/** Called when the menu appears / disappears (sounds, animations). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Menu")
	void OnMenuShown();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Menu")
	void OnMenuHidden();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	TObjectPtr<UIsdkInteractableWidgetComponent> MenuWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	TSubclassOf<UUserWidget> MenuWidgetClass;

	/** Widget resolution in pixels. World size = DrawSize * WidgetScale (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	FVector2D DrawSize = FVector2D(500.0, 700.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	float WidgetScale = 0.035f;

	/** Hand that holds the menu; the other hand presses the buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu")
	bool bAttachToLeftHand = true;

	/** Menu offset from the palm center (cm): toward the thumb side, along the palm normal, along the fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Placement")
	float ThumbSideOffset = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Placement")
	float PalmNormalOffset = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Placement")
	float AlongFingersOffset = 4.0f;

	/** Extra rotation of the widget relative to "facing the head" (if the widget appears mirrored, use Yaw 180). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Placement")
	FRotator WidgetRotationOffset = FRotator::ZeroRotator;

	/** How fast the menu follows the hand (0 = rigidly attached). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Placement", meta = (ClampMin = "0.0"))
	float FollowSpeed = 20.0f;

	/** Cosine of the angle between the palm normal and the direction to the head. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ShowPalmFacing = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float HidePalmFacing = 0.25f;

	/** Maximum angle (degrees) between the view direction and the direction to the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ShowGazeAngle = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HideGazeAngle = 60.0f;

	/** The hide condition must hold this long (s) before the menu disappears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection", meta = (ClampMin = "0.0"))
	float HideDelay = 0.35f;

	/** Draw the palm normal and the detection state (also: console variable handmenu.Debug 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand Menu|Detection")
	bool bDrawDebug = false;

private:
	struct FHandFrame
	{
		FVector Palm;
		FVector PalmNormal;   // out of the palm
		FVector ThumbSide;    // from the little finger toward the index finger
		FVector FingerDir;    // from the wrist toward the fingers
	};

	UIsdkHandMeshComponent* FindHandMesh() const;
	bool GetHandFrame(FHandFrame& OutFrame) const;
	bool GetHeadPose(FVector& OutLocation, FVector& OutForward) const;
	void SetMenuVisible(bool bVisible);
	FTransform ComputeMenuTransform(const FHandFrame& Hand, const FVector& HeadLocation) const;

	mutable TWeakObjectPtr<UIsdkHandMeshComponent> CachedHandMesh;

	bool bMenuVisible = false;
	float HideTimer = 0.0f;
};
