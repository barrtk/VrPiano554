#include "FallingBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PianoActor.h"

AFallingBlock::AFallingBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = BlockMesh;

    BlockMesh->SetGenerateOverlapEvents(true);
    BlockMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    BlockMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
    BlockMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    BlockMesh->SetSimulatePhysics(false);

	bIsPaused = false;
    bIsInRainMode = false;
    NotePlayTime = 0.f;
    SpawnTime = 0.f;
}

void AFallingBlock::BeginPlay()
{
	Super::BeginPlay();
    BlockMesh->OnComponentBeginOverlap.AddDynamic(this, &AFallingBlock::OnBlockOverlapBegin);
}

void AFallingBlock::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AFallingBlock::UpdatePosition(float CurrentGlobalTime, float InFallSpeed, float InStartHeight)
{
    if (bIsPaused || SpawnTime <= 0.f) return;

    if (CurrentGlobalTime >= SpawnTime)
    {
        const float TimeSinceSpawn = CurrentGlobalTime - SpawnTime;
        const float DistanceFallen = TimeSinceSpawn * InFallSpeed;
        FVector NewLocation = GetActorLocation();
        NewLocation.Z = InStartHeight - DistanceFallen;
        SetActorLocation(NewLocation);

        UE_LOG(LogTemp, Log, TEXT("Block %d (Note %d): CurrentTime=%.2f, SpawnTime=%.2f, StartHeight=%.2f, NewZ=%.2f"), 
            SequenceNumber, MidiNote, CurrentGlobalTime, SpawnTime, InStartHeight, NewLocation.Z);
    }
}

void AFallingBlock::InitBlock(int32 InMidiNote, int32 InSequenceNumber, float InNoteDuration, float InNotePlayTime, float InSpawnTime, const FTransform& InTargetKeyTransform, float InTargetKeyWidth, APianoActor* InPianoActor, const FString& InNoteName, bool bInIsLearningMode, bool bInIsRainMode)
{
    MidiNote = InMidiNote;
    SequenceNumber = InSequenceNumber;
    NoteDuration = InNoteDuration;
    NotePlayTime = InNotePlayTime;
    SpawnTime = InSpawnTime;
    TargetKeyTransform = InTargetKeyTransform;
    TargetKeyWidth = InTargetKeyWidth;
    PianoActorRef = InPianoActor;
    NoteName = InNoteName;
    bIsLearningMode = bInIsLearningMode;
    bIsInRainMode = bInIsRainMode;

#if WITH_EDITOR
    SetActorLabel(FString::Printf(TEXT("Block_%d_%s"), SequenceNumber, *InNoteName));
#endif

    if (BlockMesh && DefaultBlockMesh && BlockMesh->GetStaticMesh() == nullptr)
    {
        BlockMesh->SetStaticMesh(DefaultBlockMesh);
    }
}

void AFallingBlock::UpdateBlockScale(float InFallSpeed, float InNoteDuration)
{
	if (BlockMesh && BlockMesh->GetStaticMesh())
	{
		const FVector MeshSize = BlockMesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0f;

		if (MeshSize.Y <= 0.0f || MeshSize.X <= 0.0f || MeshSize.Z <= 0.0f) return;

		const float DesiredLength = FMath::Max(InFallSpeed * InNoteDuration, 1.0f);

        FVector NewScale = FVector(
            (TargetKeyWidth / MeshSize.X) * WidthScaleMultiplier,
            DepthScale / MeshSize.Y,
            (DesiredLength / MeshSize.Z) * LengthScaleMultiplier
        );

		SetActorScale3D(NewScale);
	}
}

void AFallingBlock::OnBlockOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    SetLifeSpan(PostCollisionLifeSpan);
    this->SetActorEnableCollision(false);
}

void AFallingBlock::PauseBlock() { bIsPaused = true; }
void AFallingBlock::ResumeBlock() { bIsPaused = false; }
