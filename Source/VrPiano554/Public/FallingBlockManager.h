#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Networking.h"
#include "FallingBlockManager.generated.h"

class AFallingBlock;
class FUdpSocketReceiver;

UCLASS()
class VRPIANO554_API AFallingBlockManager : public AActor
{
    GENERATED_BODY()

public:
    AFallingBlockManager();
    virtual ~AFallingBlockManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    TSubclassOf<AFallingBlock> BlockClass;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float StartHeight = 200.f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float FallSpeed = 200.f;

    /** Port UDP do nasłuchiwania (np. 5005) */
    UPROPERTY(EditAnywhere, Category = "Networking")
    int32 ListenPort = 5008; // Changed port to 5008 to avoid conflict

private:
    int32 NextBlockIndex;
    float SongStartTime;

    // UDP
    FSocket* ListenSocket;
    FUdpSocketReceiver* UDPReceiver;

    FCriticalSection ArrivalTimesMutex;
    TArray<float> ArrivalTimes;

    void StartUDPListener();
    void OnUDPMessageReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
};