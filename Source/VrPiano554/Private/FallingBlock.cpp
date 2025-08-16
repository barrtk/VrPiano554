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
		if (MeshSize.Y <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("AFallingBlock: StaticMesh Y-axis size is zero, cannot scale properly."));
			return;
		}

		// 2. Calculate the desired length of the block based on duration and speed.
		const float DesiredLength = FMath::Max(FallSpeed * NoteDuration, 1.0f); // Ensure a minimum length

		// 3. Calculate the new scale. We only modify the Y-axis.
		FVector CurrentScale = GetActorScale3D();
		// We divide by MeshSize.Y to make the scaling independent of the original mesh's length.
		FVector NewScale = FVector(CurrentScale.X, (DesiredLength / MeshSize.Y), CurrentScale.Z);

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

void AFallingBlock::InitBlock(float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight)
{
	NoteDuration = InNoteDuration;
	FallSpeed = InFallSpeed;
	StartHeight = InStartHeight;
	TargetZHeight = InTargetZHeight; // Assign the new parameter

	// Set the initial location
	FVector Location = GetActorLocation();
	Location.Z = StartHeight;
	SetActorLocation(Location);
}