#include "FallingBlock.h"
#include "FallingBlockManager.h"
#include "Components/StaticMeshComponent.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

	MidiNote = 0;
    Manager = nullptr;
    TargetTime = 0.0f;
	BlockScaleMultiplier = FVector(1.0f, 1.0f, 1.0f);
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
	// We disable ticking here and let the manager call our Tick function manually.
	// This gives the manager more control over the update order.
	SetActorTickEnabled(false);
}

void AFallingBlock::Initialize(AFallingBlockManager* InManager, float InTargetTime, float InDuration, float InKeyWidth, int32 InMidiNote)
{
    Manager = InManager;
    TargetTime = InTargetTime;
    MidiNote = InMidiNote;

    if (!Manager)
    {
        return;
    }

	// --- Dynamic Scaling Logic ---
	const float ScaleX = 0.08f;
	const float Margin = 1.0f;
	const float ScaleY = (InKeyWidth > Margin) ? (InKeyWidth - Margin) / 100.0f : 0.1f;
	// The length of the block is its duration in seconds multiplied by the highway speed.
	const float ScaleZ = (Manager->UnitsPerSecond * InDuration) / 100.0f; // 100.0f is the default mesh size

	FVector FinalScale = FVector(ScaleX, ScaleY, FMath::Max(ScaleZ, 0.01f)) * BlockScaleMultiplier;
	BlockMesh->SetWorldScale3D(FinalScale);
}

void AFallingBlock::Tick(float DeltaTime)
{
	// This Tick is called manually by the FallingBlockManager

	if (!Manager)
	{
		return;
	}

    const float CurrentMasterTime = Manager->GetCurrentSongTime();

	// Destroy the block if it's far past its target time.
	// The grace period of 2 seconds ensures it's not destroyed while potentially still needed for learning mode.
	if (CurrentMasterTime > TargetTime + 2.0f)
	{
		Destroy();
		return;
	}

	// Calculate the Z offset based on the time difference and the highway speed.
	const float TimeDiff = TargetTime - CurrentMasterTime;
	const float ZOffset = TimeDiff * Manager->UnitsPerSecond;

	// The final position is the key's base location plus the calculated Z offset.
	const FVector NewLocation = TargetKeyLocation + (TargetKeyUpVector * ZOffset);
	SetActorLocation(NewLocation);
}