#include "FallingBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

	// Default values are now set in the header
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld()->GetTimeSeconds();

	// --- Robust Scaling Logic ---
	if (BlockMesh && BlockMesh->GetStaticMesh())
	{
		// 1. Get the original size of the mesh from the StaticMesh asset itself.
		const FVector MeshSize = BlockMesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0f;

		// Avoid division by zero if the mesh is somehow sizeless
		if (MeshSize.Y <= 0.0f || MeshSize.X <= 0.0f) // Also check X for width scaling
		{
			UE_LOG(LogTemp, Warning, TEXT("AFallingBlock: StaticMesh X or Y-axis size is zero, cannot scale properly."));
			return;
		}

		// 2. Calculate the desired length of the block based on duration and speed.
		const float DesiredLength = FMath::Max(FallSpeed * NoteDuration, 1.0f); // Ensure a minimum length

		// 3. Calculate the new scale. We modify X (width) and Y (length).
		FVector CurrentScale = GetActorScale3D();
		FVector NewScale = FVector(
            TargetKeyWidth / MeshSize.X, // Scale X to match key width
            DesiredLength / MeshSize.Y,  // Scale Y for length
            CurrentScale.Z               // Keep Z scale as is (height of block)
        );

		// 4. Apply the new scale.
		SetActorScale3D(NewScale);
	}
}

void AFallingBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Location = GetActorLocation();
	Location.Z -= FallSpeed * DeltaTime;
	SetActorLocation(Location);

	// Usuwamy klocek, jeśli jest poniżej TargetZHeight (z małym marginesem)
	if (Location.Z < TargetZHeight - 5.0f)
	{
		Destroy();
	}
}

void AFallingBlock::InitBlock(float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight, const FTransform& InTargetKeyTransform, float InTargetKeyWidth)
{
	NoteDuration = InNoteDuration;
	FallSpeed = InFallSpeed;
	StartHeight = InStartHeight;
	TargetZHeight = InTargetZHeight; // Assign the new parameter
    TargetKeyTransform = InTargetKeyTransform;
    TargetKeyWidth = InTargetKeyWidth;

	// Set the initial location and rotation based on the key transform
	SetActorLocation(TargetKeyTransform.GetLocation() + FVector(0,0,StartHeight)); // Spawn above the key
    SetActorRotation(TargetKeyTransform.GetRotation()); // Inherit key's rotation
}