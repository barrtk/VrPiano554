#include "FallingBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Math/UnrealMathUtility.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

	MovementSpeed = 0.0f;
	bIsActive = false;
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
}

void AFallingBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsActive)
	{
		return;
	}

	// Move towards the target with constant speed using manual vector math
	FVector CurrentLocation = GetActorLocation();
    float DistanceToTarget = FVector::Dist(CurrentLocation, TargetLocation);
    float DistanceToMove = MovementSpeed * DeltaTime;

    if (DistanceToMove >= DistanceToTarget)
    {
        // If we are close enough, just snap to the target and destroy
        SetActorLocation(TargetLocation);
        Destroy();
    }
    else
    {
        // Otherwise, move along the direction vector
        FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
        FVector NewLocation = CurrentLocation + Direction * DistanceToMove;
        SetActorLocation(NewLocation);
    }
}

void AFallingBlock::Initialize(const FVector& InTargetLocation, float InSpeed, float InDuration, float InKeyWidth)
{
	TargetLocation = InTargetLocation;
	MovementSpeed = InSpeed;

	// --- Dynamic Scaling Logic ---
	// This assumes the base mesh is a 100x100x100 unit cube.
	// Axis mapping assumption: X=Depth, Y=Width, Z=Height/Length

	// Set a fixed, small depth for the block to make it appear flat.
	float ScaleX = 0.08f; // e.g., 8 units deep

	// Calculate width based on the key's width.
	// A small margin is subtracted for better visual separation between adjacent blocks.
	float Margin = 1.0f;
	float ScaleY = (InKeyWidth > Margin) ? (InKeyWidth - Margin) / 100.0f : 0.1f;

	// Calculate height (length) based on speed and note duration.
	float ScaleZ = (MovementSpeed > 0 && InDuration > 0) ? (MovementSpeed * InDuration) / 100.0f : 0.2f;

	BlockMesh->SetWorldScale3D(FVector(ScaleX, ScaleY, ScaleZ));

	bIsActive = true;
}

void AFallingBlock::PauseBlock()
{
	bIsActive = false;
}

void AFallingBlock::ResumeBlock()
{
	bIsActive = true;
}
