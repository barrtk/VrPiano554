#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingBlock.generated.h"

class APianoActor;

UCLASS()
class VRPIANO554_API AFallingBlock : public AActor
{
    GENERATED_BODY()

public:
    AFallingBlock();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    /** Initializes the block, setting its duration, speed, start position, and target Z height. */
    void InitBlock(int32 InMidiNote, int32 InSequenceNumber, float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight, const FTransform& InTargetKeyTransform, float InTargetKeyWidth, APianoActor* InPianoActor, const FString& InNoteName, bool bInIsLearningMode, bool bInIsRainMode);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    bool bIsLearningMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    int32 SequenceNumber;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    FString NoteName;

    void UpdateBlockScale();

    void PauseBlock();
    void ResumeBlock();

    UFUNCTION()
    void OnBlockOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


    // --- Właściwości widoczne w edytorze do łatwiejszego debugowania ---

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float StartHeight = 200.f;

    // The Z-coordinate where the block should be destroyed (e.g., the top of the piano keys).
    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float TargetZHeight = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks", meta = (DisplayName = "Post-Collision Life Span"))
    float PostCollisionLifeSpan = 0.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    int32 MidiNote;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    float NoteDuration = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    float FallSpeed = 200.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    FTransform TargetKeyTransform;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
    float TargetKeyWidth;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    UStaticMesh* DefaultBlockMesh; // Add this property

    UPROPERTY(EditAnywhere, Category = "Falling Blocks|Scaling")
    float WidthScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks|Scaling")
    float LengthScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks|Scaling")
    float DepthScale = 0.1f;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent* BlockMesh;

    APianoActor* PianoActorRef;

    float SpawnTime;
    bool bIsPaused;
    bool bIsInRainMode;
};
