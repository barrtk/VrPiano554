#include "FallingBlockManager.h"
#include "FallingBlock.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"

AFallingBlockManager::AFallingBlockManager()
{
    PrimaryActorTick.bCanEverTick = true;
    NextBlockIndex = 0;
    SongStartTime = 0.f;
    ListenSocket = nullptr;
    UDPReceiver = nullptr;
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

    float CurrentTime = GetWorld()->GetTimeSeconds() - SongStartTime;

    ArrivalTimesMutex.Lock();
    ArrivalTimes.Sort();
    while (NextBlockIndex < ArrivalTimes.Num() && CurrentTime >= ArrivalTimes[NextBlockIndex])
    {
        FVector SpawnLocation = GetActorLocation();
        SpawnLocation.Z = StartHeight;

        AFallingBlock* NewBlock = GetWorld()->SpawnActor<AFallingBlock>(BlockClass, SpawnLocation, FRotator::ZeroRotator);
        if (NewBlock)
        {
            NewBlock->InitBlock(ArrivalTimes[NextBlockIndex], FallSpeed, StartHeight, NewBlock->TargetZHeight);
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
    FString ReceivedStr;
    *Data << ReceivedStr;

    float NewTime = FCString::Atof(*ReceivedStr);
    if (NewTime > 0)
    {
        FScopeLock Lock(&ArrivalTimesMutex);
        ArrivalTimes.Add(NewTime);
    }
}
