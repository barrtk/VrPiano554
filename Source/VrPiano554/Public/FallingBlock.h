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
	void Initialize(const FVector& InTargetLocation, float InSpeed, float InDuration, float InKeyWidth);

	// Pause and Resume movement
	void PauseBlock();
	void ResumeBlock();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BlockMesh;

	// Target location for the block to reach
	FVector TargetLocation;

	// Speed at which the block moves
	float MovementSpeed;

	// Whether the block is currently moving
	bool bIsActive;
};