#include "FallingBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PianoActor.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

    // Revert to original collision settings for overlap events
    BlockMesh->SetGenerateOverlapEvents(true);
    BlockMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    BlockMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
    BlockMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    // Disable physics simulation
    BlockMesh->SetSimulatePhysics(false);

	bIsPaused = false;
    bIsInRainMode = false; // Initialize rain mode
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
	SpawnTime = GetWorld()->GetTimeSeconds();

    // Register the overlap event
    BlockMesh->OnComponentBeginOverlap.AddDynamic(this, &AFallingBlock::OnBlockOverlapBegin);
}

void AFallingBlock::UpdateBlockScale()
{
	// --- Robust Scaling Logic ---
	if (BlockMesh && BlockMesh->GetStaticMesh())
	{
		const FVector MeshSize = BlockMesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0f;

		if (MeshSize.Y <= 0.0f || MeshSize.X <= 0.0f || MeshSize.Z <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("AFallingBlock: StaticMesh has a zero dimension, cannot scale properly."));
			return;
		}

		const float DesiredLength = FMath::Max(FallSpeed * NoteDuration, 1.0f);

        // X = Width, Y = Depth, Z = Height/Length
		FVector NewScale = FVector(
            (TargetKeyWidth / MeshSize.X) * WidthScaleMultiplier,
            DepthScale / MeshSize.Y,
            (DesiredLength / MeshSize.Z) * LengthScaleMultiplier
        );

		SetActorScale3D(NewScale);
	}
}

void AFallingBlock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPaused)
	{
		return;
	}

	FVector Location = GetActorLocation();
	Location.Z -= FallSpeed * DeltaTime;
	SetActorLocation(Location);

	// Destroy the block if it falls far below the target
	if (Location.Z < TargetZHeight - 100.0f)
	{
		Destroy();
	}
}

void AFallingBlock::InitBlock(int32 InMidiNote, int32 InSequenceNumber, float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight, const FTransform& InTargetKeyTransform, float InTargetKeyWidth, APianoActor* InPianoActor, const FString& InNoteName, bool bInIsLearningMode, bool bInIsRainMode)
{
    MidiNote = InMidiNote;
    SequenceNumber = InSequenceNumber;
	NoteDuration = InNoteDuration;
	FallSpeed = InFallSpeed;
	StartHeight = InStartHeight;
	TargetZHeight = InTargetZHeight;
    TargetKeyTransform = InTargetKeyTransform;
    TargetKeyWidth = InTargetKeyWidth;
    PianoActorRef = InPianoActor;
    NoteName = InNoteName; // Set the new property
    bIsLearningMode = bInIsLearningMode; // Store learning mode state
    bIsInRainMode = bInIsRainMode; // Store rain mode state

#if WITH_EDITOR
    // Set the actor's label for debugging
    SetActorLabel(FString::Printf(TEXT("Block_%d_%s"), SequenceNumber, *InNoteName));
#endif

	SetActorLocation(TargetKeyTransform.GetLocation() + FVector(0,0,StartHeight));
    SetActorRotation(TargetKeyTransform.GetRotation());

    if (BlockMesh && DefaultBlockMesh && BlockMesh->GetStaticMesh() == nullptr)
    {
        BlockMesh->SetStaticMesh(DefaultBlockMesh);
    }

    UpdateBlockScale();
}

void AFallingBlock::OnBlockOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Stop the block from falling further
    FallSpeed = 0.0f;

    if (PianoActorRef)
    {
        if (bIsInRainMode)
        {
            // In Rain Mode, just highlight the key
            PianoActorRef->HighlightKeyForDuration(MidiNote, 0.5f); // Duration is configurable in FallingBlockManager
        }
        else
        {
            // In normal or learning mode, play the note animation
            PianoActorRef->PlayNote(MidiNote, NoteDuration);
        }
    }

    // Set a lifespan for the block to be destroyed after a delay
    // This allows the player to see the block on the key for a moment
    SetLifeSpan(PostCollisionLifeSpan);
}

void AFallingBlock::PauseBlock()
{
	bIsPaused = true;
}

void AFallingBlock::ResumeBlock()
{
	bIsPaused = false;
}
