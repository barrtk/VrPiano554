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

void AFallingBlock::InitBlock(int32 InMidiNote, int32 InSequenceNumber, float InNoteDuration, float InFallSpeed, float InStartHeight, float InTargetZHeight, const FTransform& InTargetKeyTransform, float InTargetKeyWidth, APianoActor* InPianoActor, const FString& InNoteName, bool bInIsLearningMode)
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

    // Check if we have a valid PianoActor reference
    if (PianoActorRef)
    {
        // We could check if OtherActor is the PianoActor, but it's more robust to check if the overlapped component is a key.
        // For now, we assume any overlap that stops the block should trigger the note.
        PianoActorRef->PlayNote(MidiNote, NoteDuration);
    }

    if (!bIsLearningMode)
    {
        // If not in learning mode, destroy immediately
        Destroy();
    }
    else
    {
        // If in learning mode, set a lifespan for delayed destruction
        SetLifeSpan(2.0f);
    }
}

void AFallingBlock::PauseBlock()
{
	bIsPaused = true;
}

void AFallingBlock::ResumeBlock()
{
	bIsPaused = false;
}