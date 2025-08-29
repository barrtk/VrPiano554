#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Networking.h"
#include "FallingBlockManager.generated.h"

class AFallingBlock;
class FUdpSocketReceiver;
class APianoActor; // Forward declaration
class AVrPianoPawn; // Forward declaration

// Struct to hold block spawn information (time and MIDI note)
USTRUCT(BlueprintType)
struct FBlockSpawnInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Falling Block Info")
    float Time;

    UPROPERTY(BlueprintReadWrite, Category = "Falling Block Info")
    int32 MidiNote;

    UPROPERTY(BlueprintReadWrite, Category = "Falling Block Info")
    float Duration;

    // Default constructor
    FBlockSpawnInfo() : Time(0.0f), MidiNote(0), Duration(0.0f) {}

    // Constructor with parameters
    FBlockSpawnInfo(float InTime, int32 InMidiNote, float InDuration) : Time(InTime), MidiNote(InMidiNote), Duration(InDuration) {}

    // Comparison operator for sorting
    bool operator<(const FBlockSpawnInfo& Other) const
    {
        return Time < Other.Time;
    }
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

    UFUNCTION(BlueprintCallable, Category = "Falling Blocks")
    float GetCurrentSongTime() const { return CurrentSongTime; }

    UFUNCTION(BlueprintCallable, Category = "Falling Blocks")
    void SetSongTime(float Time);

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    TSubclassOf<AFallingBlock> BlockClass;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float StartHeight = 200.f;

	// This is the time in seconds before the note is scheduled to be hit that the block will appear.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falling Blocks", meta = (DisplayName = "Lookahead Time"))
	float LookaheadTime = 3.0f;

    UPROPERTY(EditAnywhere, Category = "Falling Blocks")
    float TargetZHeight = 50.f;

	/** The visual speed of the falling blocks, in Unreal Units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falling Blocks", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float UnitsPerSecond = 100.0f;

    /** Port UDP do nasłuchiwania (np. 5005) */
    UPROPERTY(EditAnywhere, Category = "Networking")
    int32 ListenPort = 5008; // Changed port to 5008 to avoid conflict

    void OnUDPMessageReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

private:
    int32 NextSpawnIndex;
    int32 NextHighlightIndex;
    float CurrentSongTime;

    // UDP
    FSocket* ListenSocket;
    FUdpSocketReceiver* UDPReceiver;

    FCriticalSection ArrivalTimesMutex;
    TArray<FBlockSpawnInfo> ArrivalTimes;
    TArray<AFallingBlock*> ActiveBlocks;

    void StartUDPListener();


    APianoActor* PianoActorRef;
    AVrPianoPawn* VrPianoPawnRef;

    void PopulateKeyData();
    void SetMidiData(const TArray<FBlockSpawnInfo>& NewArrivalTimes);
	void SpawnBlockForNote(const FBlockSpawnInfo& NoteInfo);


    UFUNCTION()
    void OnPianoKeysInitialized();

    UFUNCTION()
    void OnPianoCalibrationComplete();

    // Handles the event when a note is played on the piano
    UFUNCTION()
    void OnNotePlayed(int32 MidiNote);

    // Handles the event when the learning mode state changes
    UFUNCTION()
    void OnLearningModeChanged(bool bNewState);

    TMap<int32, FTransform> KeyRelativeTransforms;
    TMap<int32, float> KeyWidths;

    // Set of MIDI notes that are currently waiting for the player to press in learning mode
    TSet<int32> WaitingNotes;

    bool bIsCurrentlyPaused;
    bool bHasPopulatedKeyData;
    bool bIsMovementPaused; // Tracks if blocks are frozen for learning mode
};