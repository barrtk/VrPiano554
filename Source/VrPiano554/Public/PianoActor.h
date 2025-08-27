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
class FSocket; // Forward declaration for FSocket

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuToggled, bool, bIsMenuVisible);

// Delegate for pause state changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPauseStateChanged, bool, bNewPauseState);

// Delegate for learning mode state changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLearningModeStateChanged, bool, bNewLearningModeState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerNotePlayed, int32, MidiNote);

// Delegates for Mute and Life Hold
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFileMuteStateChanged, bool, bNewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLiveMuteStateChanged, bool, bNewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLifeHoldStateChanged, bool, bNewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFileAnimationMuteStateChanged, bool, bNewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnKeysInitialized);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCalibrationComplete);


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
    
    UFUNCTION(BlueprintCallable, Category = "MIDI")
    void HandleMidiEventWithSource(int32 Note, bool bIsNoteOn, const FString& Source);

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void PrevMidi();

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void NextMidi();

    void SetLeftCalibrationPoint();
    void SetRightCalibrationPoint();
    void ApplyCalibration();
    void PressKey(int32 MidiNote);
    void ReleaseKey(int32 MidiNote);

    // Functions to handle highlighting keys
    void HighlightKeys(const TArray<int32>& NotesToHighlight);
    void UnhighlightKeys(const TArray<int32>& NotesToUnhighlight);
    void HighlightKeyForDuration(int32 MidiNote, float Duration);


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

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void TogglePauseState();

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void ToggleLearningMode();

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void ToggleFileMute();

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void ToggleLiveMute();

    UFUNCTION(BlueprintCallable, Category = "Piano|Menu")
    void ToggleLifeHold();

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void MidiSlower();

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void MidiFaster();

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void UnmuteAll();

    UFUNCTION(BlueprintCallable, Category = "Piano|MIDI")
    void ToggleLoop();

    UFUNCTION(BlueprintCallable, Category = "Piano|Game")
    void StartRestart();

	UFUNCTION(BlueprintCallable, Category = "Piano")
    void ToggleFileAnimationMute();
    //~ End Menu Functions

    /**
     * Retrieves the world transform and width of a specific piano key.
     * @param MidiNote The MIDI note number of the key (e.g., 60 for Middle C).
     * @param OutTransform The world transform (location, rotation, scale) of the key's pivot.
     * @param OutWidth The width of the key in Unreal units.
     * @return True if the key was found, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "Piano|Keys")
    bool GetKeyTransformAndWidth(int32 MidiNote, FTransform& OutTransform, float& OutWidth);

    UFUNCTION(BlueprintCallable, Category = "Piano|Keys")
    void PlayNote(int32 MidiNote, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Piano")
    void BroadcastKeysInitialized();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // Added for socket cleanup
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

    UPROPERTY(BlueprintReadWrite, Category = "Menu")
    bool bIsPaused;

    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnPauseStateChanged OnPauseStateChanged;

    UPROPERTY(BlueprintReadWrite, Category = "Menu")
    bool bIsLearningMode;

    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnLearningModeStateChanged OnLearningModeStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Piano|MIDI")
    FOnPlayerNotePlayed OnPlayerNotePlayed;

    UPROPERTY(BlueprintReadWrite, Category = "Menu")
    bool bIsFileMuted;

    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnFileMuteStateChanged OnFileMuteStateChanged;

    UPROPERTY(BlueprintReadWrite, Category = "Menu")
    bool bIsLiveMuted;

    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnLiveMuteStateChanged OnLiveMuteStateChanged;

    UPROPERTY(BlueprintReadWrite, Category = "Menu")
    bool bIsLifeHoldActive;

    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnLifeHoldStateChanged OnLifeHoldStateChanged;

    UPROPERTY(BlueprintReadWrite, Category = "Piano")
    bool bIsFileAnimationMuted = false;

    UPROPERTY(BlueprintAssignable, Category = "Piano")
    FOnFileAnimationMuteStateChanged OnFileAnimationMuteStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Piano")
    FOnKeysInitialized OnKeysInitialized;

    UPROPERTY(BlueprintAssignable, Category = "Piano|Calibration")
    FOnCalibrationComplete OnCalibrationComplete;
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
    TMap<int32, FTimerHandle> KeyHighlightTimers;


    // Offset calculated at runtime to center the piano model
    FVector CalculatedOffset;

private:
    void OnRightTriggerPressed();
    void OnRightTriggerReleased();
    void OnLeftTriggerPressed();
    void OnLeftTriggerReleased();

    // UDP Socket for sending commands
    FSocket* SenderSocket; // Added
    void SendUDPCommand(const FString& Command); // Added

public: // Moved from private
    static FString GetNoteName(int32 MidiNote);
};
