#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingBlock.generated.h"

class AFallingBlockManager; // Forward declaration

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
	void Initialize(AFallingBlockManager* InManager, const FVector& InSpawnLocation, const FVector& InTargetLocation, float InTargetTime, float InDuration, float InKeyWidth, int32 InMidiNote);

	// The MIDI note this block corresponds to
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Block")
	int32 MidiNote;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BlockMesh;

	// Pointer to the manager to get the master song time
	UPROPERTY()
	AFallingBlockManager* Manager;

	// The start and end points of the fall
	FVector SpawnLocation;
	FVector FinalTargetLocation;

	// The start and end times of the fall, based on the song's timeline
	float StartTime;
	float TargetTime;

	// Whether the block has reached its destination and should stop ticking
	bool bHasReachedTarget;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falling Block", meta = (AllowPrivateAccess = "true"))
	FVector BlockScaleMultiplier;
};
