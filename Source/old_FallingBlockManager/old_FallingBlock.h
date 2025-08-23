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

	/** Initializes the block, setting its duration, speed, start position, target Z height and MIDI note pitch. */
	void InitBlock(float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight, int32 InNotePitch);

	// --- Practice Mode ---
	void PauseBlock();
	void ResumeBlock();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	int32 NotePitch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	bool bIsPaused;

	// --- Właściwości widoczne w edytorze do łatwiejszego debugowania ---

	UPROPERTY(EditAnywhere, Category = "Falling Blocks")
	float StartHeight = 200.f;

	// The Z-coordinate where the block should be destroyed (e.g., the top of the piano keys).
	UPROPERTY(EditAnywhere, Category = "Falling Blocks")
	float TargetZHeight = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	float NoteDuration = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	float FallSpeed = 200.f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* BlockMesh;

	float SpawnTime;
};