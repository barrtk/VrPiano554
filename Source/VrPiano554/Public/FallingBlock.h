#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingBlock.generated.h"

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

	// Initializes the block's movement and appearance
	void Initialize(const FVector& InTargetLocation, float InSpeed, float InDuration, float InKeyWidth, int32 InMidiNote, bool bInIsLearningMode);

	// Pause and Resume movement
	void PauseBlock();
	void ResumeBlock();

	// Sets the learning mode state for a block that is already active
	void SetLearningMode(bool bNewState);

	// The MIDI note this block corresponds to
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Block")
	int32 MidiNote;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BlockMesh;

	// Target location for the block to reach
	FVector TargetLocation;

	// Speed at which the block moves
	float MovementSpeed;

	// Whether the block is currently moving
	bool bIsActive;

	// True if the block should wait at the target instead of being destroyed
	bool bIsLearningMode;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falling Block", meta = (AllowPrivateAccess = "true"))
	FVector BlockScaleMultiplier;
};