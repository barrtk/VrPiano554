// PianoActor.h

#pragma once

#include "VrPiano554.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"
#include "PianoActor.generated.h"

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

    ECalibrationState CalibrationState;
    FTransform LeftCalibrationTransform;
    FTransform RightCalibrationTransform;
    TMap<int32, USceneComponent*> KeyPivotMap;
    TMap<int32, float> ActiveKeyAnimations;

    // Map to store original materials of highlighted keys
    TMap<int32, UMaterialInterface*> OriginalKeyMaterials;

    // Offset calculated at runtime to center the piano model
    FVector CalculatedOffset;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* DebugCylinder;
};
