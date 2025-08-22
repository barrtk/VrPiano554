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

AFallingBlockManager::AFallingBlockManager()
{
    PrimaryActorTick.bCanEverTick = true;
    NextBlockIndex = 0;
    SongStartTime = 0.f;
    ListenSocket = nullptr;
    UDPReceiver = nullptr;
    PianoActorRef = nullptr; // Initialize
    VrPianoPawnRef = nullptr; // Initialize
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
    SongStartTime = GetWorld()->GetTimeSeconds();
    StartUDPListener();

    // Find PianoActor
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APianoActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        PianoActorRef = Cast<APianoActor>(FoundActors[0]);
        UE_LOG(LogTemp, Log, TEXT("FallingBlockManager: Found PianoActor."));
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

    if (!BlockClass) return;

    // Pause logic
    if (PianoActorRef && PianoActorRef->bIsPaused)
    {
        return; // Do not spawn or move blocks if paused
    }

    float CurrentTime = GetWorld()->GetTimeSeconds() - SongStartTime;

    ArrivalTimesMutex.Lock();
    ArrivalTimes.Sort();
    while (NextBlockIndex < ArrivalTimes.Num() && CurrentTime >= ArrivalTimes[NextBlockIndex].Time) // Use .Time
    {
        FVector SpawnLocation = GetActorLocation();
        SpawnLocation.Z = StartHeight;

        int32 MidiNote = ArrivalTimes[NextBlockIndex].MidiNote; // Get MIDI note from FBlockSpawnInfo

        FTransform KeyTransform = FTransform::Identity;
        float KeyWidth = 0.0f;

        if (PianoActorRef && PianoActorRef->GetKeyTransformAndWidth(MidiNote, KeyTransform, KeyWidth))
        {
            // Calculate rotation to face the player
            FRotator BlockRotation = FRotator::ZeroRotator;
            if (VrPianoPawnRef && VrPianoPawnRef->CameraComponent)
            {
                FVector PlayerLocation = VrPianoPawnRef->CameraComponent->GetComponentLocation();
                FVector BlockLocation = KeyTransform.GetLocation(); // Block spawns at key location
                FVector DirectionToPlayer = (PlayerLocation - BlockLocation).GetSafeNormal();
                BlockRotation = DirectionToPlayer.Rotation();
            }

            AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, KeyTransform.GetLocation(), BlockRotation);
            if (NewBlock)
            {
                NewBlock->InitBlock(ArrivalTimes[NextBlockIndex].Time, FallSpeed, StartHeight, NewBlock->TargetZHeight, KeyTransform, KeyWidth);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("FallingBlockManager: Could not get key transform or width for MIDI note %d. Spawning at default location."), MidiNote);
            AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, FRotator::ZeroRotator);
            if (NewBlock)
            {
                NewBlock->InitBlock(ArrivalTimes[NextBlockIndex].Time, FallSpeed, StartHeight, NewBlock->TargetZHeight, FTransform::Identity, 10.0f); // Default width
            }
        }

        NextBlockIndex++;
    }
    ArrivalTimesMutex.Unlock();
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
    FString JsonString;
    *Data << JsonString;

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        float Time = 0.0f;
        int32 MidiNote = 0;

        if (JsonObject->TryGetNumberField(TEXT("time"), Time) && JsonObject->TryGetNumberField(TEXT("midi_note"), MidiNote))
        {
            if (Time > 0)
            {
                FScopeLock Lock(&ArrivalTimesMutex);
                ArrivalTimes.Add(FBlockSpawnInfo(Time, MidiNote));
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
