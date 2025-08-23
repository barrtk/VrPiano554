#include "FallingBlockManager.h"
#include "FallingBlock.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Kismet/GameplayStatics.h" // For GetAllActorsOfClass
#include "PianoActor.h" // For APianoActor
#include "VrPianoPawn.h" // For AVrPianoPawn
#include "Json.h" // For FJsonSerializer, FJsonObject
#include "JsonUtilities.h" // For FJsonUtilities

void AFallingBlockManager::PopulateKeyData()
{
    if (!PianoActorRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: PianoActorRef is not set. Cannot populate key data."));
        return;
    }

    KeyTransforms.Empty();
    KeyWidths.Empty();

    for (int32 MidiNote = 0; MidiNote < 128; ++MidiNote)
    {
        FTransform KeyTransform;
        float KeyWidth;
        if (PianoActorRef->GetKeyTransformAndWidth(MidiNote, KeyTransform, KeyWidth))
        {
            KeyTransforms.Add(MidiNote, KeyTransform);
            KeyWidths.Add(MidiNote, KeyWidth);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Populated data for %d keys."), KeyTransforms.Num());
}

AFallingBlockManager::AFallingBlockManager()
{
    PrimaryActorTick.bCanEverTick = true;
    NextBlockIndex = 0;
    CurrentSongTime = 0.f;
    ListenSocket = nullptr;
    UDPReceiver = nullptr;
    PianoActorRef = nullptr; // Initialize
    VrPianoPawnRef = nullptr; // Initialize
	bIsCurrentlyPaused = false;
}

AFallingBlockManager::~AFallingBlockManager()
{
    if (UDPReceiver)
    {
        UDPReceiver->Stop();
        delete UDPReceiver;
        UDPReceiver = nullptr;
    }
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

void AFallingBlockManager::BeginPlay()
{
    Super::BeginPlay();
    StartUDPListener();

    // Find PianoActor
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APianoActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        PianoActorRef = Cast<APianoActor>(FoundActors[0]);
        UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Found PianoActor."));
        // Bind to PianoActor's OnKeysInitialized event
        if (PianoActorRef)
        {
            PianoActorRef->OnKeysInitialized.AddDynamic(this, &AFallingBlockManager::OnPianoKeysInitialized);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: PianoActor not found!"));
    }

    // Find VrPianoPawn
    FoundActors.Empty();
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVrPianoPawn::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        VrPianoPawnRef = Cast<AVrPianoPawn>(FoundActors[0]);
        UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Found VrPianoPawn."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: VrPianoPawn not found!"));
    }
}

void AFallingBlockManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (UDPReceiver)
    {
        UDPReceiver->Stop();
        delete UDPReceiver;
        UDPReceiver = nullptr;
    }
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

void AFallingBlockManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!BlockClass || FallSpeed <= 0.0f) return;

    // --- Pause Logic ---
    bool bIsNowPaused = (PianoActorRef && PianoActorRef->bIsPaused);
    if (bIsNowPaused != bIsCurrentlyPaused)
    {
        bIsCurrentlyPaused = bIsNowPaused;

        TArray<AActor*> FoundBlocks;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFallingBlock::StaticClass(), FoundBlocks);

        for (AActor* BlockActor : FoundBlocks)
        {
            AFallingBlock* Block = Cast<AFallingBlock>(BlockActor);
            if (Block)
            {
                if (bIsCurrentlyPaused)
                {
                    Block->PauseBlock();
                }
                else
                {
                    Block->ResumeBlock();
                }
            }
        }
    }

    if (bIsCurrentlyPaused)
    {
        return; // Do not advance time or spawn blocks if paused
    }

    // --- Time and Spawning Logic ---
    CurrentSongTime += DeltaTime;

    // Only process one block per tick to simplify logic and avoid race conditions
    if (NextBlockIndex < ArrivalTimes.Num())
    {
        const float FallTime = (StartHeight - TargetZHeight) / FallSpeed;
        const FBlockSpawnInfo& CurrentNoteInfo = ArrivalTimes[NextBlockIndex];
        const float NotePlayTime = CurrentNoteInfo.Time;
        const float BlockSpawnTime = NotePlayTime - FallTime;

        if (CurrentSongTime >= BlockSpawnTime)
        {
            UE_LOG(LogTemp, Log, TEXT("Tick: Spawning block for MidiNote: %d, BlockSpawnTime: %f"), CurrentNoteInfo.MidiNote, BlockSpawnTime);
            int32 MidiNote = CurrentNoteInfo.MidiNote;

            const FTransform* KeyTransformPtr = KeyTransforms.Find(MidiNote);
            const float* KeyWidthPtr = KeyWidths.Find(MidiNote);

            if (KeyTransformPtr && KeyWidthPtr)
            {
                const FTransform& KeyTransform = *KeyTransformPtr;
                const float KeyWidth = *KeyWidthPtr;

                FRotator BlockRotation = FRotator::ZeroRotator;
                if (VrPianoPawnRef && VrPianoPawnRef->CameraComponent)
                {
                    FVector PlayerLocation = VrPianoPawnRef->CameraComponent->GetComponentLocation();
                    FVector BlockLocation = KeyTransform.GetLocation();
                    FVector DirectionToPlayer = (PlayerLocation - BlockLocation).GetSafeNormal();
                    BlockRotation = DirectionToPlayer.Rotation();
                }

                AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, KeyTransform.GetLocation(), BlockRotation);
                if (NewBlock)
                {
                    NewBlock->InitBlock(MidiNote, NextBlockIndex, CurrentNoteInfo.Duration, FallSpeed, StartHeight, TargetZHeight, KeyTransform, KeyWidth, PianoActorRef, APianoActor::GetNoteName(MidiNote));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Could not find key data for MIDI note %d. Block will not be spawned."), MidiNote);
            }
            NextBlockIndex++;
        }
    }
}

void AFallingBlockManager::OnPianoKeysInitialized()
{
    UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received OnPianoKeysInitialized event. Populating key data."));
    PopulateKeyData();
}

void AFallingBlockManager::SetSongTime(float Time)
{
    CurrentSongTime = Time;

    FScopeLock Lock(&ArrivalTimesMutex);
    NextBlockIndex = 0;
    for (int32 i = 0; i < ArrivalTimes.Num(); ++i)
    {
        if (ArrivalTimes[i].Time >= CurrentSongTime)
        {
            NextBlockIndex = i;
            break;
        }
    }
}

void AFallingBlockManager::StartUDPListener()
{
    ListenSocket = FUdpSocketBuilder(TEXT("UDP_Listener"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(ListenPort)
        .WithReceiveBufferSize(1024);

    if (ListenSocket)
    {
        UDPReceiver = new FUdpSocketReceiver(ListenSocket, FTimespan::FromMilliseconds(10), TEXT("UDP_Receiver"));
        UDPReceiver->OnDataReceived().BindUObject(this, &AFallingBlockManager::OnUDPMessageReceived);
        UDPReceiver->Start();
    }
}

void AFallingBlockManager::OnUDPMessageReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    FString JsonString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data->GetData())));

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        double Time = 0.0;
        int32 MidiNote = 0;
        double Duration = 0.5; // Default duration

        if (JsonObject->TryGetNumberField(TEXT("time"), Time) && 
            JsonObject->TryGetNumberField(TEXT("midi_note"), MidiNote))
        {
            JsonObject->TryGetNumberField(TEXT("duration"), Duration);

            if (Time > 0)
            {
                FScopeLock Lock(&ArrivalTimesMutex);
                ArrivalTimes.Add(FBlockSpawnInfo(Time, MidiNote, Duration));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Received UDP JSON missing 'time' or 'midi_note' field: %s"), *JsonString);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FallingBlockManager: Failed to parse UDP JSON: %s"), *JsonString);
    }
}

void AFallingBlockManager::SetMidiData(const TArray<FBlockSpawnInfo>& NewArrivalTimes)
{
    FScopeLock Lock(&ArrivalTimesMutex);
    ArrivalTimes = NewArrivalTimes;
    NextBlockIndex = 0;
    CurrentSongTime = 0.0f;
}