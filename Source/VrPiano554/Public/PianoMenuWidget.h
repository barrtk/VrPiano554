// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#ifndef VRPiano554_API
#define VRPiano554_API __declspec(dllexport)
#endif

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PianoActor.h" // Include PianoActor.h directly
#include "PianoMenuWidget.generated.h"

class UButton; // Forward declaration for UButton
class UTextBlock; // Forward declaration for UTextBlock
class FSocket; // Forward declaration for FSocket

/**
 *
 */
UCLASS()
class VRPiano554_API UPianoMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Properties for buttons used in NativeConstruct
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_1;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_2;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_3;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_4;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_5;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_6;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_8;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_9;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_10;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_12;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_13;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_14;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_15;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_16;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_20; // Prev Midi
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UButton* Button_21; // Next Midi

    // Properties for text blocks used in NativeConstruct
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UTextBlock* aktualneMidi;
    
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    class UTextBlock* midiTempo;

protected:
    virtual void NativeConstruct() override;
    virtual void BeginDestroy() override; // Added BeginDestroy declaration

    UFUNCTION()
    void OnStartButtonClicked();

    // PianoActor property
    UPROPERTY()
    class APianoActor* PianoActor;

    // UFUNCTIONS for button clicks (as seen in .cpp)
    UFUNCTION()
    void OnButton_KalibracjaXMniejClicked();
    UFUNCTION()
    void OnButton_KalibracjaXWiecejClicked();
    UFUNCTION()
    void OnButton_KalibracjaYMniejClicked();
    UFUNCTION()
    void OnButton_KalibracjaYWiecejClicked();
    UFUNCTION()
    void OnButton_KalibracjaZMniejClicked();
    UFUNCTION()
    void OnButton_KalibracjaZWiecejClicked();
    UFUNCTION()
    void OnButton_ResetClicked();
    UFUNCTION()
    void OnButton_StartRestartClicked();
    UFUNCTION()
    void OnButton_PauzaClicked();
    UFUNCTION()
    void OnButton_TrybNaukiClicked();
    UFUNCTION()
    void OnButton_MidiWolniejClicked();
    UFUNCTION()
    void OnButton_MidiSzybciejClicked();
    UFUNCTION()
    void OnButton_MuteFileClicked();
    UFUNCTION()
    void OnButton_MuteLiveClicked();
    UFUNCTION()
    void OnButton_ToggleLoopClicked();
    UFUNCTION()
    void OnButton_PrevMidiClicked(); // New Prev Midi button handler
    UFUNCTION()
    void OnButton_NextMidiClicked(); // New Next Midi button handler

    UFUNCTION()
    void OnButton_ToggleFileAnimationMuteClicked();

    UFUNCTION()
    void HandleFileAnimationMuteStateChanged(bool bNewState);

    // Handlers for PianoActor state changes
    UFUNCTION()
    void HandlePauseStateChanged(bool bNewPauseState);
    UFUNCTION()
    void HandleLearningModeStateChanged(bool bNewLearningModeState);
    UFUNCTION()
    void HandleFileMuteStateChanged(bool bNewFileMuteState);
    UFUNCTION()
    void HandleLiveMuteStateChanged(bool bNewLiveMuteState);
    UFUNCTION()
    void HandleMidiTempoChanged(int32 NewTempo); // New MidiTempo handler

private:
    bool bIsPauzaActive;

public: // Moved to public section for external access
    UFUNCTION(BlueprintCallable, Category = "UDP")
    void ReceiveUDPData(const FString& Data);

    
    void UpdateMidiText(const FString& MidiMessage);
    // Corrected signature for UpdateButtonState
    void UpdateButtonState(const FString& ButtonName, bool bIsActive);

private:
    // Socket declaration (removed from here, now in PianoActor)
    // FSocket* Socket;

    // SendUDPCommand declaration (removed from here, now in PianoActor)
    // void SendUDPCommand(const FString& Command);
};

// Removed FUdpReceiverWorker from here, now encapsulated in .cpp
// class FUdpReceiverWorker;
