#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "VrPianoPawn.generated.h"

UCLASS()
class VRPIANO554_API AVrPianoPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AVrPianoPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Root for tracking */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	USceneComponent* VRTrackingCenter;

	/** Camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	UCameraComponent* CameraComponent;

	/** Left motion controller */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	UMotionControllerComponent* LeftController;

	/** Right motion controller */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	UMotionControllerComponent* RightController;

	/** Widget interaction for UI pointing (attached to RightController) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI Interaction")
	UWidgetInteractionComponent* WidgetInteractionComponent;

	/** Visual laser pointer (child of WidgetInteraction) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI Interaction")
	UStaticMeshComponent* LaserPointerMesh;

protected:
	// Input handlers
	void OnLeftTriggerPressed();
	void OnLeftTriggerReleased();
	void OnRightTriggerPressed();
	void OnRightTriggerReleased();

	// Update laser mesh position/scale
	void UpdateLaserPointer();
};
