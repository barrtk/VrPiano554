#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PianoSaveGame.generated.h"

UCLASS()
class VRPIANO554_API UPianoSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = Basic)
	FTransform PianoTransform;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	FString SaveSlotName;

	UPROPERTY(VisibleAnywhere, Category = Basic)
	uint32 UserIndex;

	UPianoSaveGame();
};
