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
    NextSpawnIndex = 0;
    NextHighlightIndex = 0;
    CurrentSongTime = 0.f;
    ListenSocket = nullptr;
    UDPReceiver = nullptr;
    PianoActorRef = nullptr; 
    VrPianoPawnRef = nullptr; 
	bIsCurrentlyPaused = false;
    bRainMode = false; 
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

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APianoActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        PianoActorRef = Cast<APianoActor>(FoundActors[0]);
        UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Found PianoActor."));
        if (PianoActorRef)
        {
            PianoActorRef->OnKeysInitialized.AddDynamic(this, &AFallingBlockManager::OnPianoKeysInitialized);
            PianoActorRef->OnCalibrationComplete.AddDynamic(this, &AFallingBlockManager::OnPianoCalibrationComplete);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: PianoActor not found!"));
    }

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

    if (!BlockClass || FallSpeed <= 0.0f || !PianoActorRef)
    {
        return;
    }

    bool bIsNowPaused = PianoActorRef->bIsPaused;
    if (bIsNowPaused != bIsCurrentlyPaused)
    {
        bIsCurrentlyPaused = bIsNowPaused;
        for (AFallingBlock* Block : ActiveBlocks)
        {
            if (IsValid(Block))
            {
                bIsCurrentlyPaused ? Block->PauseBlock() : Block->ResumeBlock();
            }
        }
    }

    if (bIsCurrentlyPaused)
    {
        return; 
    }

    CurrentSongTime += DeltaTime;

    // --- 1. Update existing blocks ---
    for (int32 i = ActiveBlocks.Num() - 1; i >= 0; --i)
    {
        AFallingBlock* Block = ActiveBlocks[i];
        if (IsValid(Block))
        {
            Block->UpdatePosition(CurrentSongTime, FallSpeed, StartHeight);
        }
        else
        {
            ActiveBlocks.RemoveAt(i);
        }
    }

    // --- 2. Spawn new blocks ---
    if (NextSpawnIndex < ArrivalTimes.Num())
    {
        const float FallTime = (StartHeight - TargetZHeight) / FallSpeed;
        const FBlockSpawnInfo& NoteInfo = ArrivalTimes[NextSpawnIndex];
        const float BlockSpawnTime = NoteInfo.Time - FallTime;

        if (CurrentSongTime >= BlockSpawnTime)
        {
            const int32 MidiNote = NoteInfo.MidiNote;
            const FTransform* KeyTransformPtr = KeyTransforms.Find(MidiNote);
            const float* KeyWidthPtr = KeyWidths.Find(MidiNote);

            if (KeyTransformPtr && KeyWidthPtr)
            {
                FVector SpawnLocation = FVector(KeyTransformPtr->GetLocation().X, KeyTransformPtr->GetLocation().Y, StartHeight);
                FRotator SpawnRotation = PianoActorRef->GetActorRotation();
                AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, SpawnRotation);
                if (NewBlock)
                {
                    NewBlock->InitBlock(MidiNote, NextSpawnIndex, NoteInfo.Duration, NoteInfo.Time, BlockSpawnTime, *KeyTransformPtr, *KeyWidthPtr, PianoActorRef, APianoActor::GetNoteName(MidiNote), PianoActorRef->bIsLearningMode, bRainMode);
                    NewBlock->UpdateBlockScale(FallSpeed, NoteInfo.Duration);
                    ActiveBlocks.Add(NewBlock);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Could not find key data for MIDI note %d. Block will not be spawned."), MidiNote);
            }
            NextSpawnIndex++;
        }
    }

    // --- 3. Trigger highlights (in rain mode) ---
    if (bRainMode && NextHighlightIndex < ArrivalTimes.Num())
    {
        const FBlockSpawnInfo& NoteInfo = ArrivalTimes[NextHighlightIndex];
        if (CurrentSongTime >= NoteInfo.Time)
        {
            PianoActorRef->HighlightKeyForDuration(NoteInfo.MidiNote, RainModeKeyHighlightDuration);
            NextHighlightIndex++;
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

    for (AFallingBlock* Block : ActiveBlocks)
    {
        if(IsValid(Block)) Block->Destroy();
    }
    ActiveBlocks.Empty();

    FScopeLock Lock(&ArrivalTimesMutex);
    NextSpawnIndex = 0;
    NextHighlightIndex = 0;
    for (int32 i = 0; i < ArrivalTimes.Num(); ++i)
    {
        if (ArrivalTimes[i].Time >= CurrentSongTime)
        {
            NextSpawnIndex = i;
            NextHighlightIndex = i;
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
    FString ReceivedString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data->GetData())));

    if (ReceivedString.TrimStartAndEnd().Equals(TEXT("/rain"), ESearchCase::IgnoreCase))
    {
        ToggleRainMode(!bRainMode);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        double Time = 0.0;
        int32 MidiNote = 0;
        double Duration = 0.5; 

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
            UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Received UDP JSON missing 'time' or 'midi_note' field: %s"), *ReceivedString);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FallingBlockManager: Failed to parse UDP JSON: %s"), *ReceivedString);
    }
}

void AFallingBlockManager::OnPianoCalibrationComplete()
{
    UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received OnCalibrationComplete event. Re-populating key data."));
    PopulateKeyData();
}

void AFallingBlockManager::SetMidiData(const TArray<FBlockSpawnInfo>& NewArrivalTimes)
{
    FScopeLock Lock(&ArrivalTimesMutex);
    ArrivalTimes = NewArrivalTimes;
    
    for (AFallingBlock* Block : ActiveBlocks)
    {
        if(IsValid(Block)) Block->Destroy();
    }
    ActiveBlocks.Empty();

    NextSpawnIndex = 0;
    NextHighlightIndex = 0;
    CurrentSongTime = 0.0f;
}

void AFallingBlockManager::ToggleRainMode(bool bIsEnabled)
{
    bRainMode = bIsEnabled;
    FString Status = bRainMode ? TEXT("ENABLED") : TEXT("DISABLED");
    UE_LOG(LogTemp, Warning, TEXT("Rain Mode has been %s"), *Status);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Rain Mode: %s"), *Status));
    }
}
