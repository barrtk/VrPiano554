// PianoActor.h

#pragma once

#include "VrPiano554.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "PianoSaveGame.h"
#include "PianoActor.generated.h"

class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuToggled, bool, bIsMenuVisible);

UENUM(BlueprintType)
enum class ECalibrationState : uint8
{
    Idle,
    WaitingForLeftPoint,
    WaitingForRightPoint
};

UCLASS()
class VRPIANO554_API APianoActor : public AActor
{
    GENERATED_BODY()

public:
    APianoActor();

    UFUNCTION(BlueprintCallable, Category = "Piano|Calibration")
    void StartCalibration();

    UFUNCTION(BlueprintCallable, Category = "MIDI")
    void HandleMidiNote(int32 Note, bool bIsNoteOn);

    void SetLeftCalibrationPoint();
    void SetRightCalibrationPoint();
    void ApplyCalibration();
    void PressKey(int32 MidiNote);
    void ReleaseKey(int32 MidiNote);

    // Functions to handle highlighting keys
    void HighlightKeys(const TArray<int32>& NotesToHighlight);
    void UnhighlightKeys(const TArray<int32>& NotesToUnhighlight);

    //~ Begin Menu Functions
    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void AdjustPositionX(float Value);

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void AdjustPositionY(float Value);

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void AdjustPositionZ(float Value);

    UFUNCTION(BlueprintCallable, Category = "Piano|SaveLoad")
    void SavePosition();

    UFUNCTION(BlueprintCallable, Category = "Piano|SaveLoad")
    void LoadPosition();

    UFUNCTION(BlueprintCallable, Category = "Piano|SaveLoad")
    void ResetPosition();
    //~ End Menu Functions

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    


public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano Setup|Materials")
    UMaterialInterface* WhiteKeyMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano Setup|Materials")
    UMaterialInterface* BlackKeyMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano Setup|Materials")
    UMaterialInterface* HighlightedKeyMaterial;

    //~ Begin Menu Properties
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piano|Menu")
    UWidgetComponent* MenuWidgetComponent;

    UPROPERTY(BlueprintAssignable, Category = "Piano|Menu")
    FOnMenuToggled OnMenuToggled;
    //~ End Menu Properties

    // Widget interaction for UI pointing (attached to RightController)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI Interaction")
    UWidgetInteractionComponent* WidgetInteractionComponent;

    UPROPERTY()
    UPianoSaveGame* LoadGameInstance;

    UPROPERTY()
    class UPianoMenuWidget* PianoMenuWidgetInstance;

private:
    UPROPERTY(VisibleAnywhere)
    USceneComponent* StableRoot;

    UPROPERTY(VisibleAnywhere)
    UMotionControllerComponent* LeftController;

    UPROPERTY(VisibleAnywhere)
    UMotionControllerComponent* RightController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* Coffre;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* Potards;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* PotardCentral;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* Pads;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* Molettes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* LCD;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* CurseursNew;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* Connectique;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TMap<int32, UStaticMeshComponent*> KeyMeshComponents;

public:
    UPROPERTY(EditAnywhere, Category = "Piano Setup")
    float PianoModelWidth = 122.0f;

    UPROPERTY(EditAnywhere, Category = "Piano Setup")
    float AnimationSpeed = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano Setup")
    float TargetRotationAngle = 7.0f;

private:
    void LoadMidiFile();
    void SetupControllers();
    void ToggleMenu();

    ECalibrationState CalibrationState;
    FTransform LeftCalibrationTransform;
    FTransform RightCalibrationTransform;
    TMap<int32, USceneComponent*> KeyPivotMap;
    TMap<int32, float> ActiveKeyAnimations;

    // Map to store original materials of highlighted keys
    TMap<int32, UMaterialInterface*> OriginalKeyMaterials;

    // Offset calculated at runtime to center the piano model
    FVector CalculatedOffset;

private:
    void OnRightTriggerPressed();
    void OnRightTriggerReleased();
    void OnLeftTriggerPressed();
    void OnLeftTriggerReleased();

};