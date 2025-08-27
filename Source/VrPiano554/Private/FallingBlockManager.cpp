#include "FallingBlockManager.h"
#include "FallingBlock.h"
#include "PianoActor.h"
#include "VrPianoPawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Containers/StringConv.h" // Required for FUTF8ToTCHAR

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

    // Find PianoActor and VrPianoPawn
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APianoActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        PianoActorRef = Cast<APianoActor>(FoundActors[0]);
        if (PianoActorRef)
        {
            UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Found PianoActor."));
            PianoActorRef->OnKeysInitialized.AddDynamic(this, &AFallingBlockManager::OnPianoKeysInitialized);
            PianoActorRef->OnCalibrationComplete.AddDynamic(this, &AFallingBlockManager::OnPianoCalibrationComplete);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: PianoActor not found!"));
    }

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
    // Cleanup UDP
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

    if (!BlockClass || !PianoActorRef || !bHasPopulatedKeyData)
    {
        return;
    }

    // Check for pause or learning mode
    bool bShouldBePaused = PianoActorRef->bIsPaused || PianoActorRef->bIsLearningMode;

    // If the pause state changed, update all active blocks
    if (bShouldBePaused != bIsCurrentlyPaused)
    {
        bIsCurrentlyPaused = bShouldBePaused;
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

    // --- 1. Spawn new blocks ---
    while (NextSpawnIndex < ArrivalTimes.Num())
    {
        const FBlockSpawnInfo& NoteInfo = ArrivalTimes[NextSpawnIndex];
        // Spawn condition based on LookaheadTime
        if (CurrentSongTime >= NoteInfo.Time - LookaheadTime)
        {
            SpawnBlockForNote(NoteInfo);
            NextSpawnIndex++;
        }
        else
        {
            // Notes are sorted, so we can break early
            break;
        }
    }

    // --- 2. Trigger highlights and sounds ---
    while (NextHighlightIndex < ArrivalTimes.Num())
    {
        const FBlockSpawnInfo& NoteInfo = ArrivalTimes[NextHighlightIndex];
        if (CurrentSongTime >= NoteInfo.Time)
        {
            if (bRainMode)
            {
                PianoActorRef->HighlightKeyForDuration(NoteInfo.MidiNote, RainModeKeyHighlightDuration);
            }
            else
            {
                // In normal mode, play the note sound and animation
                PianoActorRef->PlayNote(NoteInfo.MidiNote, true);
            }
            NextHighlightIndex++;
        }
        else
        {
            // Notes are sorted, so we can break early
            break;
        }
    }
	
    // --- 3. Update existing blocks (they move themselves now) ---
	for (int32 i = ActiveBlocks.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(ActiveBlocks[i]))
        {
            ActiveBlocks.RemoveAt(i);
        }
    }
}

void AFallingBlockManager::SpawnBlockForNote(const FBlockSpawnInfo& NoteInfo)
{
    const int32 MidiNote = NoteInfo.MidiNote;
    const FTransform* KeyTransformPtr = KeyTransforms.Find(MidiNote);
    const float* KeyWidthPtr = KeyWidths.Find(MidiNote);

    if (KeyTransformPtr && KeyWidthPtr)
    {
        FVector KeyLocation = KeyTransformPtr->GetLocation();
        FVector SpawnLocation = FVector(KeyLocation.X, KeyLocation.Y, StartHeight);
        FVector TargetLocation = FVector(KeyLocation.X, KeyLocation.Y, TargetZHeight);
        FRotator SpawnRotation = PianoActorRef->GetActorRotation();

        AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, SpawnRotation);
        if (NewBlock)
        {
            // Calculate dynamic speed
            float Distance = FVector::Dist(SpawnLocation, TargetLocation);
            float Speed = (LookaheadTime > 0) ? Distance / LookaheadTime : 0.0f;

            NewBlock->Initialize(TargetLocation, Speed, NoteInfo.Duration, *KeyWidthPtr);
            
            ActiveBlocks.Add(NewBlock);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Could not find key data for MIDI note %d. Block will not be spawned."), MidiNote);
    }
}


void AFallingBlockManager::OnPianoKeysInitialized()
{
    UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received OnPianoKeysInitialized event. Populating key data."));
    PopulateKeyData();
	bHasPopulatedKeyData = true;
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
		if (i == ArrivalTimes.Num() - 1)
		{
			// If we reached the end, set indices to the end
			NextSpawnIndex = ArrivalTimes.Num();
			NextHighlightIndex = ArrivalTimes.Num();
		}
    }
}

void AFallingBlockManager::StartUDPListener()
{
    ListenSocket = FUdpSocketBuilder(TEXT("UDP_Listener"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(ListenPort)
        .WithReceiveBufferSize(2 * 1024 * 1024); // Increased buffer size for larger MIDI files

    if (ListenSocket)
    {
        UDPReceiver = new FUdpSocketReceiver(ListenSocket, FTimespan::FromMilliseconds(10), TEXT("UDP_Receiver"));
        UDPReceiver->OnDataReceived().BindUObject(this, &AFallingBlockManager::OnUDPMessageReceived);
        UDPReceiver->Start();
        UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: UDP Listener started on port %d."), ListenPort);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FallingBlockManager: Failed to create UDP Listener on port %d."), ListenPort);
    }
}

void AFallingBlockManager::OnUDPMessageReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    // Use FUTF8ToTCHAR to safely convert a sized buffer from UTF-8 to TCHAR
    FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(Data->GetData()), Data->Num());
    const FString ReceivedString(Converter.Length(), Converter.Get());

    if (ReceivedString.TrimStartAndEnd().Equals(TEXT("/rain"), ESearchCase::IgnoreCase))
    {
        ToggleRainMode(!bRainMode);
        return;
    }
	
    if (ReceivedString.TrimStartAndEnd().Equals(TEXT("/start_song"), ESearchCase::IgnoreCase))
    {
		UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received /start_song command. Resetting state."));
        SetMidiData(this->ArrivalTimes); // Reset song with currently buffered notes
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        TArray<FBlockSpawnInfo> NewNotes;
        const TArray<TSharedPtr<FJsonValue>>* NotesJsonArray;

        if (JsonObject->TryGetArrayField(TEXT("notes"), NotesJsonArray))
        {
            // We received a full song
            for (const auto& Value : *NotesJsonArray)
            {
                const TSharedPtr<FJsonObject>& NoteObject = Value->AsObject();
                if (NoteObject.IsValid())
                {
                    NewNotes.Add(FBlockSpawnInfo(
                        NoteObject->GetNumberField(TEXT("time")),
                        NoteObject->GetIntegerField(TEXT("midi_note")),
                        NoteObject->GetNumberField(TEXT("duration"))
                    ));
                }
            }
            UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received full song with %d notes."), NewNotes.Num());
            SetMidiData(NewNotes);
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
    
    // Sort the array by time to ensure correct processing order
    ArrivalTimes.Sort([](const FBlockSpawnInfo& A, const FBlockSpawnInfo& B) {
        return A.Time < B.Time;
    });

    for (AFallingBlock* Block : ActiveBlocks)
    {
        if(IsValid(Block)) Block->Destroy();
    }
    ActiveBlocks.Empty();

    NextSpawnIndex = 0;
    NextHighlightIndex = 0;
    CurrentSongTime = 0.0f;
	
	UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: MIDI data set and sorted. Ready to play."));
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