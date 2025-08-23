#include "FallingBlockManager.h"
#include "FallingBlock.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/NameTypes.h"
#include "Kismet/GameplayStatics.h"


AFallingBlockManager::AFallingBlockManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // Note Data Listener
    NoteListenSocket = nullptr;
    NoteUDPReceiver = nullptr;

    // Command Listener
    CommandListenSocket = nullptr;
    CommandUDPReceiver = nullptr;

    // State
    NextBlockIndex = 0;
    SongStartTime = 0.f;
    PlaybackSpeed = 1.0f;
    bIsInPracticeMode = false;
    bIsWaitingForKey = false;
    bIsPaused = false;
    PianoActor = nullptr;
}

AFallingBlockManager::~AFallingBlockManager()
{
    // Clean up both receivers
    if (NoteUDPReceiver)
    {
        NoteUDPReceiver->Stop();
        delete NoteUDPReceiver;
    }
    if (NoteListenSocket)
    {
        NoteListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(NoteListenSocket);
    }
    if (CommandUDPReceiver)
    {
        CommandUDPReceiver->Stop();
        delete CommandUDPReceiver;
    }
    if (CommandListenSocket)
    {
        CommandListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(CommandListenSocket);
    }
}

void AFallingBlockManager::BeginPlay()
{
    Super::BeginPlay();
    PopulateKeyLocations();
    StartNoteDataListener();
    StartCommandListener();
}

void AFallingBlockManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    // Destructor will handle cleanup
}

void AFallingBlockManager::UpdateKeyLocations()
{
    UE_LOG(LogTemp, Log, TEXT("Updating key locations..."));
    KeyLocations.Empty();
    PopulateKeyLocations();
}


void AFallingBlockManager::PopulateKeyLocations()
{
    if (!PianoActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("PianoActor is not set in FallingBlockManager. Cannot determine key locations."));
        return;
    }

    TArray<UStaticMeshComponent*> PianoKeyComponents;
    PianoActor->GetComponents<UStaticMeshComponent>(PianoKeyComponents);

    if (PianoKeyComponents.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No StaticMeshComponents found on PianoActor."));
        return;
    }

    const FString Prefix = TEXT("Note");
    for (UStaticMeshComponent* KeyComponent : PianoKeyComponents)
    {
        FString ComponentName = KeyComponent->GetName();
        if (ComponentName.StartsWith(Prefix))
        {
            FString NoteNumberString = ComponentName.RightChop(Prefix.Len());
            if (NoteNumberString.IsNumeric())
            {
                int32 NoteNumber = FCString::Atoi(*NoteNumberString);
                KeyLocations.Add(NoteNumber, KeyComponent->Bounds.GetBox().GetCenter());
                UE_LOG(LogTemp, Log, TEXT("Found key %d at location %s"), NoteNumber, *KeyComponent->Bounds.GetBox().GetCenter().ToString());
            }
        }
    }

    if (KeyLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No keys with the 'Note' prefix found on PianoActor."));
    }
}


void AFallingBlockManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsPaused || !BlockClass) return;


    // If we are in practice mode and waiting for a key, do not advance the song time
    if (bIsInPracticeMode && bIsWaitingForKey)
    {
        return;
    }

    // If there are no notes or we've played them all, do nothing
    if (SongNotes.Num() == 0 || NextBlockIndex >= SongNotes.Num())
    {
        return;
    }

    // Adjust current time by playback speed
    float CurrentSongTime = (GetWorld()->GetTimeSeconds() - SongStartTime) * PlaybackSpeed;

    // Lock for thread safety
    FScopeLock Lock(&SongNotesMutex);

    // Spawn all blocks whose time has come
    while (NextBlockIndex < SongNotes.Num() && CurrentSongTime >= SongNotes[NextBlockIndex].Time)
    {
        const FNoteInfo& CurrentNote = SongNotes[NextBlockIndex];

        FVector SpawnLocation = GetActorLocation(); // Default location
        const FVector* KeyLocation = KeyLocations.Find(CurrentNote.Pitch);

        if (KeyLocation)
        {
            SpawnLocation = *KeyLocation + FVector(0, 0, StartHeight);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find location for note pitch %d. Spawning at default location."), CurrentNote.Pitch);
        }

        AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, FRotator::ZeroRotator);
        if (NewBlock)
        {
            NewBlock->InitBlock(CurrentNote.Duration, FallSpeed, StartHeight, NewBlock->TargetZHeight, CurrentNote.Pitch);

            if (bIsInPracticeMode)
            {
                NewBlock->PauseBlock();
                PausedBlocks.Add(NewBlock);
            }
        }

        NextBlockIndex++;
    }

    // If we just spawned blocks in practice mode, set the waiting flag
    if (bIsInPracticeMode && PausedBlocks.Num() > 0)
    {
        bIsWaitingForKey = true;
    }
}

void AFallingBlockManager::StartNoteDataListener()
{
    NoteListenSocket = FUdpSocketBuilder(TEXT("NoteDataUDPListener"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(NoteDataListenPort)
        .WithReceiveBufferSize(65507); // Increased buffer size for full JSON

    if (NoteListenSocket)
    {
        NoteUDPReceiver = new FUdpSocketReceiver(NoteListenSocket, FTimespan::FromMilliseconds(100), TEXT("NoteData_UDPReceiver"));
        NoteUDPReceiver->OnDataReceived().BindUObject(this, &AFallingBlockManager::OnNoteDataReceived);
        NoteUDPReceiver->Start();
        UE_LOG(LogTemp, Log, TEXT("Note Data UDP Listener started on port %d"), NoteDataListenPort);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start Note Data UDP Listener on port %d"), NoteDataListenPort);
    }
}

void AFallingBlockManager::OnNoteDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    // Convert data to FString
    const FString JsonString = FString(Data->Num(), UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data->GetData())));
    UE_LOG(LogTemp, Log, TEXT("Received Note Data: %s"), *JsonString);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* NotesJsonArray;
        if (JsonObject->TryGetArrayField("notes", NotesJsonArray))
        {
            FScopeLock Lock(&SongNotesMutex);
            SongNotes.Empty();
            NextBlockIndex = 0;
            SongStartTime = GetWorld()->GetTimeSeconds(); // Reset song start time

            for (const TSharedPtr<FJsonValue>& NoteValue : *NotesJsonArray)
            {
                const TSharedPtr<FJsonObject> NoteObject = NoteValue->AsObject();
                if (NoteObject.IsValid())
                {
                    FNoteInfo NoteInfo;
                    NoteObject->TryGetNumberField("time", NoteInfo.Time);
                    NoteObject->TryGetNumberField("pitch", NoteInfo.Pitch);
                    NoteObject->TryGetNumberField("duration", NoteInfo.Duration);
                    SongNotes.Add(NoteInfo);
                }
            }
            UE_LOG(LogTemp, Log, TEXT("Successfully parsed %d notes."), SongNotes.Num());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to parse Note Data JSON."));
    }
}

void AFallingBlockManager::StartCommandListener()
{
    CommandListenSocket = FUdpSocketBuilder(TEXT("CommandUDPListener"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToPort(CommandListenPort);

    if (CommandListenSocket)
    {
        CommandUDPReceiver = new FUdpSocketReceiver(CommandListenSocket, FTimespan::FromMilliseconds(100), TEXT("Command_UDPReceiver"));
        CommandUDPReceiver->OnDataReceived().BindUObject(this, &AFallingBlockManager::OnUDPCommandReceived);
        CommandUDPReceiver->Start();
        UE_LOG(LogTemp, Log, TEXT("Command UDP Listener started on port %d"), CommandListenPort);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start Command UDP Listener on port %d"), CommandListenPort);
    }
}

void AFallingBlockManager::OnUDPCommandReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    const FString JsonString = FString(Data->Num(), UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data->GetData())));
    UE_LOG(LogTemp, Log, TEXT("Received Command: %s"), *JsonString);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString Command;
        if (JsonObject->TryGetStringField("command", Command))
        {
            if (Command == TEXT("set_speed"))
            {
                double NewSpeed;
                if (JsonObject->TryGetNumberField("value", NewSpeed))
                {
                    PlaybackSpeed = FMath::Max(0.1f, (float)NewSpeed);
                    UE_LOG(LogTemp, Log, TEXT("Set PlaybackSpeed to %f"), PlaybackSpeed);
                }
            }
            else if (Command == TEXT("set_practice_mode"))
            {
                bool bNewValue;
                if (JsonObject->TryGetBoolField("value", bNewValue))
                {
                    bIsInPracticeMode = bNewValue;
                    UE_LOG(LogTemp, Log, TEXT("Set Practice Mode to %s"), bIsInPracticeMode ? TEXT("true") : TEXT("false"));
                    
                    // If we are turning practice mode off, resume any paused blocks
                    if (!bIsInPracticeMode)
                    {
                        for (AFallingBlock* Block : PausedBlocks)
                        {
                            if(Block && Block->IsValidLowLevel())
                            {
                                Block->ResumeBlock();
                            }
                        }
                        PausedBlocks.Empty();
                        bIsWaitingForKey = false;
                    }
                }
            }
            else if (Command == TEXT("practice_key_pressed"))
            {
                int32 NotePitch;
                if (JsonObject->TryGetNumberField("note", NotePitch))
                {
                    // Find the corresponding paused block and resume it
                    for (int i = PausedBlocks.Num() - 1; i >= 0; --i)
                    {
                        AFallingBlock* Block = PausedBlocks[i];
                        if (Block && Block->IsValidLowLevel() && Block->NotePitch == NotePitch)
                        {
                            Block->ResumeBlock();
                            PausedBlocks.RemoveAt(i);
                            UE_LOG(LogTemp, Log, TEXT("Resumed block for note %d"), NotePitch);
                        }
                    }

                    // If all waiting blocks have been cleared, continue the song
                    if (PausedBlocks.Num() == 0)
                    {
                        bIsWaitingForKey = false;
                        UE_LOG(LogTemp, Log, TEXT("All waiting blocks cleared. Resuming song."));
                    }
                }
            }
        }
    }
}