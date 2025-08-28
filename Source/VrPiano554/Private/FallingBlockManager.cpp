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
    bIsMovementPaused = false;
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
            PianoActorRef->OnPlayerNotePlayed.AddDynamic(this, &AFallingBlockManager::OnNotePlayed);
            PianoActorRef->OnLearningModeStateChanged.AddDynamic(this, &AFallingBlockManager::OnLearningModeChanged);
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

    // Determine if we need to wait for player input for the learning mode.
    const bool bShouldWaitForInput = PianoActorRef->bIsLearningMode && NextHighlightIndex < ArrivalTimes.Num() && CurrentSongTime >= ArrivalTimes[NextHighlightIndex].Time;

    // Handle general pause
    if (PianoActorRef->bIsPaused)
    {
        if (!bIsCurrentlyPaused)
        {
            bIsCurrentlyPaused = true;
            for (AFallingBlock* Block : ActiveBlocks) { if (IsValid(Block)) Block->PauseBlock(); }
        }
        return; // A hard pause stops everything.
    }
    else
    {
        if (bIsCurrentlyPaused)
        {
            bIsCurrentlyPaused = false;
            // When unpausing, only resume blocks if the learning mode isn't waiting for input.
            if (!bShouldWaitForInput)
            {
                for (AFallingBlock* Block : ActiveBlocks) { if (IsValid(Block)) Block->ResumeBlock(); }
            }
        }
    }

    // --- Synthesia-style Learning Mode Logic ---

    // 1a. Pause or resume block movement based on whether we are waiting for input
    if (bShouldWaitForInput && !bIsMovementPaused)
    {
        // --- PAUSE all blocks ---
        for (AFallingBlock* Block : ActiveBlocks)
        {
            if (IsValid(Block))
            {
                Block->PauseBlock();
            }
        }
        bIsMovementPaused = true;
    }
    else if (!bShouldWaitForInput && bIsMovementPaused)
    {
        // --- RESUME all blocks ---
        for (AFallingBlock* Block : ActiveBlocks)
        {
            if (IsValid(Block))
            {
                Block->ResumeBlock();
            }
        }
        bIsMovementPaused = false;
    }

    // 2. Advance song time if we are not waiting
    if (!bShouldWaitForInput)
    {
        CurrentSongTime += DeltaTime;
    }

    // 3. Spawn new blocks from the highway
    while (NextSpawnIndex < ArrivalTimes.Num() && CurrentSongTime >= ArrivalTimes[NextSpawnIndex].Time - LookaheadTime)
    {
        SpawnBlockForNote(ArrivalTimes[NextSpawnIndex]);
        NextSpawnIndex++;
    }

    // 4. Handle notes reaching the strike zone
    while (NextHighlightIndex < ArrivalTimes.Num() && CurrentSongTime >= ArrivalTimes[NextHighlightIndex].Time)
    {
        const FBlockSpawnInfo& NoteInfo = ArrivalTimes[NextHighlightIndex];

        if (PianoActorRef->bIsLearningMode)
        {
            // In learning mode, add the note to the waiting list if it's not already there.
            if (!WaitingNotes.Contains(NoteInfo.MidiNote))
            {
                WaitingNotes.Add(NoteInfo.MidiNote);
                PianoActorRef->HighlightKeyForDuration(NoteInfo.MidiNote, 3600.0f); // Highlight indefinitely
            }
        }
        else
        {
            // In normal mode, just play the note automatically.
            PianoActorRef->PlayNote(NoteInfo.MidiNote, true);
        }

        // If we are waiting for input, stop processing more notes this frame and wait.
        // The NextHighlightIndex is NOT incremented, so we are stuck on this note.
        if (bShouldWaitForInput)
        {
            break;
        }
        else
        {
            // If not waiting, it's normal mode. Process the note and move to the next one.
            NextHighlightIndex++;
        }
    }

    // --- Common Logic: Garbage collect invalid blocks ---
    for (int32 i = ActiveBlocks.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(ActiveBlocks[i]))
        {
            ActiveBlocks.RemoveAt(i);
        }
    }
}

void AFallingBlockManager::OnLearningModeChanged(bool bNewState)
{
    // Update all existing blocks with the new learning mode state.
    for (AFallingBlock* Block : ActiveBlocks)
    {
        if (IsValid(Block))
        {
            Block->SetLearningMode(bNewState);
        }
    }

    // If we are turning learning mode OFF, clear any waiting notes and highlights.
    if (!bNewState)
    {
        if(PianoActorRef) PianoActorRef->UnhighlightKeys(WaitingNotes.Array());
        WaitingNotes.Empty();
    }
}

void AFallingBlockManager::OnNotePlayed(int32 MidiNote)
{
    if (!PianoActorRef->bIsLearningMode || !WaitingNotes.Contains(MidiNote))
    {
        return; // Ignore if not in learning mode or if it's not a note we're waiting for.
    }

    WaitingNotes.Remove(MidiNote);
    PianoActorRef->UnhighlightKeys({MidiNote}); // Turn off highlight for the correct key

    // Find and destroy the corresponding waiting block.
    for (int32 i = ActiveBlocks.Num() - 1; i >= 0; --i)
    {
        AFallingBlock* Block = ActiveBlocks[i];
        if (IsValid(Block) && Block->MidiNote == MidiNote)
        {
            Block->Destroy();
            ActiveBlocks.RemoveAt(i);
            break; 
        }
    }

    // After playing a note, check if we can advance the song's main index (NextHighlightIndex).
    // We can advance if the *next* note in the sequence is no longer in our waiting set.
    // This means we have cleared all notes for the current time step.
    while (NextHighlightIndex < ArrivalTimes.Num())
    {
        const FBlockSpawnInfo& NextNoteInfo = ArrivalTimes[NextHighlightIndex];
        if (WaitingNotes.Contains(NextNoteInfo.MidiNote))
        {
            // The next note in the sequence is part of the current chord and is still waiting to be played.
            // So, we cannot advance the main index yet.
            break;
        }
        // This note has been dealt with (either played or was never in the waiting set),
        // so we can advance the index past it.
        NextHighlightIndex++;
    }
}

void AFallingBlockManager::SpawnBlockForNote(const FBlockSpawnInfo& NoteInfo)
{
    const int32 MidiNote = NoteInfo.MidiNote;
    const FTransform* KeyRelativeTransformPtr = KeyRelativeTransforms.Find(MidiNote);
    const float* KeyWidthPtr = KeyWidths.Find(MidiNote);

    if (KeyRelativeTransformPtr && KeyWidthPtr)
    {
        const FTransform PianoWorldTransform = PianoActorRef->GetActorTransform();
        const FTransform KeyWorldTransform = *KeyRelativeTransformPtr * PianoWorldTransform;
        const FVector KeyLocation = KeyWorldTransform.GetLocation();
        const FVector KeyUpVector = KeyWorldTransform.GetUnitAxis(EAxis::Z);
        const FVector TargetLocation = KeyLocation + KeyUpVector * TargetZHeight;
        const FVector SpawnLocation = KeyLocation + KeyUpVector * StartHeight;
        FRotator SpawnRotation = KeyWorldTransform.GetRotation().Rotator();

        AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, SpawnRotation);
        if (NewBlock)
        {
            float Distance = FVector::Dist(SpawnLocation, TargetLocation);
            float Speed = (LookaheadTime > 0) ? Distance / LookaheadTime : 0.0f;
            NewBlock->Initialize(TargetLocation, Speed, NoteInfo.Duration, *KeyWidthPtr, NoteInfo.MidiNote, PianoActorRef->bIsLearningMode);
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
    WaitingNotes.Empty();

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
        .WithReceiveBufferSize(2 * 1024 * 1024);

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
    FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(Data->GetData()), Data->Num());
    const FString ReceivedString(Converter.Length(), Converter.Get());

    if (ReceivedString.TrimStartAndEnd().Equals(TEXT("/start_song"), ESearchCase::IgnoreCase))
    {
		UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Received /start_song command. Resetting state."));
        SetMidiData(this->ArrivalTimes);
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

void AFallingBlockManager::PopulateKeyData()
{
    if (!PianoActorRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: PianoActorRef is not set. Cannot populate key data."));
        return;
    }

    KeyRelativeTransforms.Empty();
    KeyWidths.Empty();

    const FTransform PianoInverseTransform = PianoActorRef->GetActorTransform().Inverse();
    const FVector PianoScale = PianoActorRef->GetActorScale3D();
    const float KeyWidthScale = PianoScale.Y;

    for (int32 MidiNote = 0; MidiNote < 128; ++MidiNote)
    {
        FTransform KeyWorldTransform;
        float KeyWidth;
        if (PianoActorRef->GetKeyTransformAndWidth(MidiNote, KeyWorldTransform, KeyWidth))
        {
            FTransform KeyRelativeTransform = KeyWorldTransform * PianoInverseTransform;
            KeyRelativeTransforms.Add(MidiNote, KeyRelativeTransform);
            KeyWidths.Add(MidiNote, KeyWidth * KeyWidthScale);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Populated data for %d keys."), KeyRelativeTransforms.Num());
}