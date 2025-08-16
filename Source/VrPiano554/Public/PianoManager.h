// PianoManager.h

#pragma once

#include "VrPiano554.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PianoActor.h" // Include our new PianoActor header
#include "PianoManager.generated.h"

class UInputComponent;
class UPianoSaveGame;


UCLASS()
class VRPIANO554_API APianoManager : public AActor
{
    GENERATED_BODY()

public:
    APianoManager();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // The Blueprint version of our Piano to spawn
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Piano")
    TSubclassOf<APianoActor> PianoActorClass;

    // Speed at which the piano scales
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano")
    float ScaleSpeed = 0.1f;

private:
    // Pointer to the piano we have spawned in the world
    UPROPERTY()
    APianoActor* SpawnedPiano;

    // --- Input Functions ---
    void SetupInput();
    void SpawnOrResetPiano();
    // The new signature for our scale function
    void ScalePiano(const struct FInputActionValue& Value);
	void SavePianoTransform();
    void LoadPianoTransform();
};