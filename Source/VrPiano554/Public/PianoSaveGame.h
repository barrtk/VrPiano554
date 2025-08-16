// PianoSaveGame.h

#pragma once

#include "VrPiano554.h"
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
    FVector CalibratedBasePosition;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float YOffset;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float BlackKeyXOffset;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float BlackKeyYOffset;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float BlackKeyZOffset;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float WhiteKeyLength;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float WhiteKeyThickness;

    UPROPERTY(VisibleAnywhere, Category = Basic)
    float BlackKeyThickness;
};