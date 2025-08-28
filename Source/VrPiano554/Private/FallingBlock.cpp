#include "FallingBlock.h"
#include "FallingBlockManager.h" // Include the manager header
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

	MidiNote = 0;
    Manager = nullptr;
    StartTime = 0.0f;
    TargetTime = 0.0f;
    bHasReachedTarget = false;
    BlockScaleMultiplier = FVector(1.0f, 1.0f, 1.0f);
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
}

void AFallingBlock::Initialize(AFallingBlockManager* InManager, const FVector& InSpawnLocation, const FVector& InTargetLocation, float InTargetTime, float InDuration, float InKeyWidth, int32 InMidiNote)
{
    Manager = InManager;
    SpawnLocation = InSpawnLocation;
    FinalTargetLocation = InTargetLocation;
    TargetTime = InTargetTime;
    MidiNote = InMidiNote;
    bHasReachedTarget = false;

    // The block starts falling LookaheadTime seconds before its target time.
    if(Manager)
    {
        StartTime = TargetTime - Manager->LookaheadTime;
    }

    // --- Dynamic Scaling Logic ---
    // We need to calculate the visual speed to determine the block's length.
    const float Distance = FVector::Dist(SpawnLocation, FinalTargetLocation);
    float VisualSpeed = 0.0f;
    if(Manager && Manager->LookaheadTime > 0)
    {
        VisualSpeed = Distance / Manager->LookaheadTime;
    }

	float ScaleX = 0.08f;
	float Margin = 1.0f;
	float ScaleY = (InKeyWidth > Margin) ? (InKeyWidth - Margin) / 100.0f : 0.1f;
	float ScaleZ = (VisualSpeed > 0 && InDuration > 0) ? (VisualSpeed * InDuration) / 100.0f : 0.2f;

	FVector FinalScale = FVector(ScaleX, ScaleY, ScaleZ) * BlockScaleMultiplier;
	BlockMesh->SetWorldScale3D(FinalScale);

    // Adjust the target location to account for the block's height so it stops at its edge.
	const float HalfHeight = 50.0f * FinalScale.Z;
	const FVector UpVector = GetActorUpVector();
	FinalTargetLocation += (UpVector * HalfHeight);
}

void AFallingBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Manager || bHasReachedTarget)
	{
		return;
	}

    const float CurrentMasterTime = Manager->GetCurrentSongTime();

    // Check if we have reached or passed the target time, with a small tolerance for floating point errors.
    if (CurrentMasterTime >= TargetTime || FMath::IsNearlyEqual(CurrentMasterTime, TargetTime))
    {
        // We've reached or passed the target time. Snap to the final location and stop ticking.
        SetActorLocation(FinalTargetLocation);
        bHasReachedTarget = true;
    }
    else if (CurrentMasterTime >= StartTime)
    {
        // We are in the falling phase. Calculate position based on time.
        const float Alpha = UKismetMathLibrary::MapRangeClamped(CurrentMasterTime, StartTime, TargetTime, 0.0f, 1.0f);
        const FVector NewLocation = FMath::Lerp(SpawnLocation, FinalTargetLocation, Alpha);
        SetActorLocation(NewLocation);
    }
    // If CurrentMasterTime < StartTime, do nothing. The block waits "off-screen" until it's time to fall.
}
