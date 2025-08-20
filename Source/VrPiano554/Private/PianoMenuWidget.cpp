#include "PianoMenuWidget.h"
#include "VrPiano554.h"
#include "Components/Button.h"
#include "PianoActor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/UdpSocketBuilder.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Optional.h" // For TOptional
#include "HAL/Runnable.h" // For FRunnable
#include "HAL/RunnableThread.h" // For FRunnableThread
#include "HAL/ThreadSafeCounter.h" // For FThreadSafeCounter
#include "HAL/PlatformProcess.h" // For FPlatformProcess::Sleep

// UDP Receiver Thread
class FUdpReceiverWorker : public FRunnable
{
public:
    FUdpReceiverWorker(FSocket* InSocket, UPianoMenuWidget* InMenuWidget)
        : Socket(InSocket)
        , MenuWidget(InMenuWidget)
        , StopTaskCounter(0)
    {
        Thread = FRunnableThread::Create(this, TEXT("FUdpReceiverWorker"), 0, TPri_Normal);
    }

    virtual ~FUdpReceiverWorker()
    {
        Stop();
        if (Thread)
        {
            Thread->WaitForCompletion();
            delete Thread;
            Thread = nullptr;
        }
    }

    virtual bool Init() override
    {
        return true;
    }

    virtual uint32 Run() override
    {
        TArray<uint8> RecvBuffer;
        RecvBuffer.SetNumUninitialized(1024);
        FIPv4Endpoint SenderEndpoint;

        while (!StopTaskCounter.GetValue())
        {
            int32 BytesRead = 0;
            TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
            if (Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *Sender))
            {
                FString ReceivedString = FString(UTF8_TO_TCHAR(RecvBuffer.GetData()));
                // Ensure the string is null-terminated for safety
                ReceivedString.LeftInline(BytesRead);
                ReceivedString.TrimEndInline();

                // Pass to game thread for UI update
                AsyncTask(ENamedThreads::GameThread, [this, ReceivedString]()
                    {
                        if (MenuWidget.IsValid())
                        {
                            MenuWidget->ReceiveUDPData(ReceivedString);
                        }
                    });
            }
            else
            {
                // Sleep briefly to prevent 100% CPU usage if no data is coming in
                FPlatformProcess::Sleep(0.01f);
            }
        }
        return 0;
    }

    virtual void Stop() override
    {
        StopTaskCounter.Increment();
    }

    void EnsureCompletion()
    {
        Stop();
        if (Thread)
        {
            Thread->WaitForCompletion();
        }
    }

private:
    FSocket* Socket;
    TWeakObjectPtr<UPianoMenuWidget> MenuWidget;
    FRunnableThread* Thread;
    FThreadSafeCounter StopTaskCounter;
};

FUdpReceiverWorker* UdpReceiverWorker = nullptr;

void UPianoMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    bIsPauzaActive = false;

    // Get a reference to the PianoActor in the world
    PianoActor = Cast<APianoActor>(UGameplayStatics::GetActorOfClass(GetWorld(), APianoActor::StaticClass()));
    if (!PianoActor)
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: Could not find APianoActor in the world!"));
    }

    // Bind buttons to functions
    if (Button) Button->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaXWiecejClicked);
    if (Button_1) Button_1->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaXMniejClicked);
    if (Button_2) Button_2->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaYMniejClicked);
    if (Button_3) Button_3->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaYWiecejClicked);
    if (Button_4) Button_4->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaZMniejClicked);
    if (Button_5) Button_5->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_KalibracjaZWiecejClicked);
    if (Button_6) Button_6->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_ResetClicked);
    if (Button_8) Button_8->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_StartRestartClicked);
    if (Button_9) Button_9->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_PauzaClicked);
    if (Button_10) Button_10->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_TrybNaukiClicked);
    if (Button_12) Button_12->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_MidiWolniejClicked);
    if (Button_13) Button_13->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_MidiSzybciejClicked);
    if (Button_14) Button_14->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_MuteFileClicked);
    if (Button_15) Button_15->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_MuteLiveClicked);
    if (Button_16) Button_16->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_ToggleLoopClicked);
    if (Button_20) Button_20->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_PrevMidiClicked);
    if (Button_21) Button_21->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton_NextMidiClicked);

    // Bind TextBlocks
    if (aktualneMidi) aktualneMidi->SetText(FText::FromString(TEXT("MIDI: Loading...")));
    if (TextBlock_25) TextBlock_25->SetText(FText::FromString(TEXT("Pos: Not Saved")));
    if (MidiTempo) MidiTempo->SetText(FText::FromString(TEXT("Tempo: 100")));

    // Create UDP socket for receiving
    FIPv4Address BindAddr = FIPv4Address::Any;
    FIPv4Endpoint Endpoint(BindAddr, 5006);

    FSocket* ReceiveSocket = FUdpSocketBuilder(TEXT("PianoMenuWidgetReceiverSocket"))
        .AsReusable()
        .BoundToEndpoint(Endpoint)
        .WithReceiveBufferSize(1024);

    if (ReceiveSocket)
    {
        UdpReceiverWorker = new FUdpReceiverWorker(ReceiveSocket, this);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create UDP Receive Socket!"));
    }

    // Bind to PianoActor delegates and initialize button states
    if (PianoActor)
    {
        PianoActor->OnPauseStateChanged.AddDynamic(this, &UPianoMenuWidget::HandlePauseStateChanged);
        PianoActor->OnLearningModeStateChanged.AddDynamic(this, &UPianoMenuWidget::HandleLearningModeStateChanged);
        PianoActor->OnFileMuteStateChanged.AddDynamic(this, &UPianoMenuWidget::HandleFileMuteStateChanged);
        PianoActor->OnLiveMuteStateChanged.AddDynamic(this, &UPianoMenuWidget::HandleLiveMuteStateChanged);
        PianoActor->OnMidiTempoChanged.AddDynamic(this, &UPianoMenuWidget::HandleMidiTempoChanged);

        // Initialize button states
        UpdateButtonState(TEXT("pauza"), PianoActor->bIsPaused);
        UpdateButtonState(TEXT("tryb_nauki"), PianoActor->bIsLearningMode);
        UpdateButtonState(TEXT("mute_file"), PianoActor->bIsFileMuted);
        UpdateButtonState(TEXT("mute_live"), PianoActor->bIsLiveMuted);
        UpdateButtonState(TEXT("toggle_loop"), false); // Assume loop is initially off
        // Initialize tempo text
        HandleMidiTempoChanged(PianoActor->CurrentMidiTempo);
    }
}

void UPianoMenuWidget::BeginDestroy()
{
    Super::BeginDestroy();

    if (UdpReceiverWorker)
    {
        UdpReceiverWorker->EnsureCompletion();
        delete UdpReceiverWorker;
        UdpReceiverWorker = nullptr;
    }
}

void UPianoMenuWidget::ReceiveUDPData(const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("ReceiveUDPData called with message: %s"), *Message);
    TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Message);
    TSharedPtr<FJsonValue> JsonValue;

    if (FJsonSerializer::Deserialize(JsonReader, JsonValue) && JsonValue.IsValid())
    {
        TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
        if (JsonObject.IsValid())
        {
            FString Command = JsonObject->GetStringField(TEXT("command"));

            if (Command == TEXT("update_button_state"))
            {
                FString ButtonName = JsonObject->GetStringField(TEXT("button"));
                bool bIsActive = JsonObject->GetBoolField(TEXT("is_active"));
                UE_LOG(LogTemp, Warning, TEXT("Received update_button_state for Button: %s, IsActive: %s"), *ButtonName, bIsActive ? TEXT("True") : TEXT("False"));
                UpdateButtonState(ButtonName, bIsActive);
            }
            else if (Command == TEXT("update_midi_info"))
            {
                FString MidiInfo = JsonObject->GetStringField(TEXT("midi_info"));
                UpdateMidiText(MidiInfo);
            }
            else if (Command == TEXT("update_position_info"))
            {
                TSharedPtr<FJsonObject> PosObject = JsonObject->GetObjectField(TEXT("position"));
                if (PosObject.IsValid())
                {
                    FVector Position;
                    Position.X = PosObject->GetNumberField(TEXT("X"));
                    Position.Y = PosObject->GetNumberField(TEXT("Y"));
                    Position.Z = PosObject->GetNumberField(TEXT("Z"));
                    UpdatePositionText(Position);
                }
            }
        }
    }
}

void UPianoMenuWidget::UpdateButtonState(const FString& ButtonName, bool bIsActive)
{
    UE_LOG(LogTemp, Warning, TEXT("UpdateButtonState called for Button: %s, IsActive: %s"), *ButtonName, bIsActive ? TEXT("True") : TEXT("False"));
    UButton* TargetButton = nullptr;

    if (ButtonName == TEXT("pauza")) TargetButton = Button_9;
    else if (ButtonName == TEXT("tryb_nauki")) TargetButton = Button_10;
    else if (ButtonName == TEXT("mute_file")) TargetButton = Button_14;
    else if (ButtonName == TEXT("mute_live")) TargetButton = Button_15;
    else if (ButtonName == TEXT("toggle_loop")) TargetButton = Button_16;

    if (TargetButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("TargetButton found: %s. Applying style."), *TargetButton->GetName());
        FButtonStyle NewStyle = TargetButton->GetStyle();
        FSlateBrush NewBrush;

        if (bIsActive)
        {
            NewBrush.TintColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); // Green for active
        }
        else
        {
            NewBrush.TintColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White for inactive
        }
        NewStyle.Normal = NewBrush;
        NewStyle.Hovered = NewBrush;
        NewStyle.Pressed = NewBrush;
        TargetButton->SetStyle(NewStyle);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TargetButton not found for ButtonName: %s"), *ButtonName);
    }
}

void UPianoMenuWidget::UpdatePositionText(const FVector& Position)
{
    if (TextBlock_25)
    {
        FString PosString = FString::Printf(TEXT("Pos: X=%.1f Y=%.1f Z=%.1f"), Position.X, Position.Y, Position.Z);
        TextBlock_25->SetText(FText::FromString(PosString));
    }
}

void UPianoMenuWidget::UpdateMidiText(const FString& MidiInfo)
{
    if (aktualneMidi)
    {
        aktualneMidi->SetText(FText::FromString(MidiInfo));
    }
}

void UPianoMenuWidget::OnButton_KalibracjaXMniejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionX(-1.f);
}

void UPianoMenuWidget::OnButton_KalibracjaXWiecejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionX(1.f);
}

void UPianoMenuWidget::OnButton_KalibracjaYMniejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionY(-1.f);
}

void UPianoMenuWidget::OnButton_KalibracjaYWiecejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionY(1.f);
}

void UPianoMenuWidget::OnButton_KalibracjaZMniejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionZ(-1.f);
}

void UPianoMenuWidget::OnButton_KalibracjaZWiecejClicked()
{
    if (PianoActor) PianoActor->AdjustPositionZ(1.f);
}

void UPianoMenuWidget::OnButton_ResetClicked()
{
    if (PianoActor) PianoActor->ResetPosition();
}

void UPianoMenuWidget::OnButton_StartRestartClicked()
{
    if (PianoActor)
    {
        PianoActor->StartRestart();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to start/restart!"));
    }
}

void UPianoMenuWidget::OnButton_PauzaClicked()
{
    if (PianoActor)
    {
        PianoActor->TogglePauseState();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to toggle pause state!"));
    }
}

void UPianoMenuWidget::OnButton_TrybNaukiClicked()
{
    if (PianoActor)
    {
        PianoActor->ToggleLearningMode();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to toggle learning mode!"));
    }
}

void UPianoMenuWidget::OnButton_MidiWolniejClicked()
{
    if (PianoActor)
    {
        PianoActor->MidiSlower();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to slow midi!"));
    }
}

void UPianoMenuWidget::OnButton_MidiSzybciejClicked()
{
    if (PianoActor)
    {
        PianoActor->MidiFaster();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to speed up midi!"));
    }
}

void UPianoMenuWidget::OnButton_MuteFileClicked()
{
    if (PianoActor)
    {
        PianoActor->ToggleFileMute();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to toggle file mute!"));
    }
}

void UPianoMenuWidget::OnButton_MuteLiveClicked()
{
    if (PianoActor)
    {
        PianoActor->ToggleLiveMute();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to toggle live mute!"));
    }
}

void UPianoMenuWidget::OnButton_ToggleLoopClicked()
{
    if (PianoActor)
    {
        PianoActor->ToggleLoop();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to toggle loop!"));
    }
}

void UPianoMenuWidget::OnButton_PrevMidiClicked()
{
    if (PianoActor)
    {
        PianoActor->PrevMidi();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to go to previous midi!"));
    }
}

void UPianoMenuWidget::OnButton_NextMidiClicked()
{
    if (PianoActor)
    {
        PianoActor->NextMidi();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PianoMenuWidget: PianoActor is null when trying to go to next midi!"));
    }
}

void UPianoMenuWidget::OnStartButtonClicked()
{
    // Implement logic for Start button click
    UE_LOG(LogTemp, Warning, TEXT("Start Button Clicked!"));
}

void UPianoMenuWidget::HandlePauseStateChanged(bool bNewPauseState)
{
    UpdateButtonState(TEXT("pauza"), bNewPauseState);
}

void UPianoMenuWidget::HandleLearningModeStateChanged(bool bNewLearningModeState)
{
    UpdateButtonState(TEXT("tryb_nauki"), bNewLearningModeState);
}

void UPianoMenuWidget::HandleFileMuteStateChanged(bool bNewFileMuteState)
{
    UpdateButtonState(TEXT("mute_file"), bNewFileMuteState);
}

void UPianoMenuWidget::HandleLiveMuteStateChanged(bool bNewLiveMuteState)
{
    UpdateButtonState(TEXT("mute_live"), bNewLiveMuteState);
}

void UPianoMenuWidget::HandleMidiTempoChanged(float NewTempo)
{
    if (MidiTempo)
    {
        FString TempoString = FString::Printf(TEXT("Tempo: %.0f"), NewTempo);
        MidiTempo->SetText(FText::FromString(TempoString));
    }
}