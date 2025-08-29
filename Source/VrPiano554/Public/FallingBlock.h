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

	// Initializes the block's core properties
	void Initialize(AFallingBlockManager* InManager, float InTargetTime, float InDuration, float InKeyWidth, int32 InMidiNote);

	// The MIDI note this block corresponds to
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Block")
	int32 MidiNote;

	// The target time of the fall, based on the song's timeline
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Block")
	float TargetTime;

	// The location of the key this block is falling towards
	FVector TargetKeyLocation;
	
	// The up vector of the key this block is falling towards
	FVector TargetKeyUpVector;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BlockMesh;

	// Pointer to the manager to get the master song time
	UPROPERTY()
	AFallingBlockManager* Manager;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falling Block", meta = (AllowPrivateAccess = "true"))
	FVector BlockScaleMultiplier;
};