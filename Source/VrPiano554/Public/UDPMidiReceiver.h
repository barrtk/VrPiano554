// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PianoActor.h"
#include "UDPMidiReceiver.generated.h"

// Delegat dla zdarzeń nutowych
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnMidiNoteSignature, int32, Note, bool, bIsNoteOn, float, Duration, const FString&, Source);

// NOWY delegat dla zdarzeń podświetlania klawiszy
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMidiHighlightSignature, const TArray<int32>&, Notes, bool, bHighlightOn);

UCLASS()
class VRPIANO554_API AUDPMidiReceiver : public AActor
{
    GENERATED_BODY()
public:
    AUDPMidiReceiver();

    // Event dispatcher dla zdarzeń nutowych
    UPROPERTY(BlueprintAssignable, Category = "MIDI Events")
    FOnMidiNoteSignature OnMidiNoteEvent;

    // NOWY event dispatcher dla zdarzeń podświetlania
    UPROPERTY(BlueprintAssignable, Category = "MIDI Events")
    FOnMidiHighlightSignature OnMidiHighlightEvent;

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

protected:
    FSocket* ListenSocket;
    APianoActor* PianoActorRef;
};