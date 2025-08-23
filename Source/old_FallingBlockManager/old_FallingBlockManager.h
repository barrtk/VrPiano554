#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Networking.h"
#include "FallingBlockManager.generated.h"

class AFallingBlock;
class FUdpSocketReceiver;

// Struct to hold detailed information about each note, must be defined before the class
USTRUCT()
struct FNoteInfo
{
    GENERATED_BODY()

    UPROPERTY()
    float Time;

    UPROPERTY()
    int32 Pitch;

    UPROPERTY()
    float Duration;
};

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

	UPROPERTY(EditAnywhere, Category = "Falling Blocks")
	float PracticeModeStartHeight = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks", meta = (DisplayName = "Piano Actor Reference"))
    AActor* PianoActor;

	/** Port UDP for receiving note data as JSON */
	UPROPERTY(EditAnywhere, Category = "Networking")
	int32 NoteDataListenPort = 5008;

	/** Port UDP for receiving commands as JSON */
	UPROPERTY(EditAnywhere, Category = "Networking")
	int32 CommandListenPort = 5009;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	float PlaybackSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Falling Blocks")
	bool bIsInPracticeMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsPaused;

public:
	UFUNCTION(BlueprintCallable, Category = "Falling Blocks")
	void UpdateKeyLocations();


private:
    void PopulateKeyLocations();

	// --- Note Data UDP ---
	FSocket* NoteListenSocket;
	FUdpSocketReceiver* NoteUDPReceiver;
	void StartNoteDataListener();
	void OnNoteDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

	// --- Command UDP ---
	FSocket* CommandListenSocket;
	FUdpSocketReceiver* CommandUDPReceiver;
	void StartCommandListener();
	void OnUDPCommandReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

	// --- State ---
	int32 NextBlockIndex;
	float SongStartTime;
	FCriticalSection SongNotesMutex;
	TArray<FNoteInfo> SongNotes;
    TMap<int32, FVector> KeyLocations;

	// --- Practice Mode State ---
	bool bIsWaitingForKey;
	TArray<AFallingBlock*> PausedBlocks;
};
