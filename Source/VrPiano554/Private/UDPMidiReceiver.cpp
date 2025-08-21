#include "UDPMidiReceiver.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet/GameplayStatics.h"

AUDPMidiReceiver::AUDPMidiReceiver()
{
    PrimaryActorTick.bCanEverTick = true;
    ListenSocket = nullptr;
    PianoActorRef = nullptr;
}

void AUDPMidiReceiver::BeginPlay()
{
    Super::BeginPlay();

    // Znajdź PianoActor na scenie
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APianoActor::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        PianoActorRef = Cast<APianoActor>(FoundActors[0]);
        UE_LOG(LogTemp, Log, TEXT("UDP Receiver: Found PianoActor."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UDP Receiver: PianoActor not found!"));
    }

    // Utwórz socket UDP
    ListenSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_DGram, TEXT("MidiReceiverSocket"), false);
    if (ListenSocket)
    {
        TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        Addr->SetAnyAddress();
        Addr->SetPort(5005);

        if (ListenSocket->Bind(*Addr))
        {
            UE_LOG(LogTemp, Log, TEXT("UDP Receiver: Socket bound to port %d."), Addr->GetPort());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to bind socket to port 55454!"));
            ListenSocket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
            ListenSocket = nullptr;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to create socket!"));
    }
}

void AUDPMidiReceiver::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!ListenSocket)
    {
        return;
    }

    uint32 Size;
    while (ListenSocket->HasPendingData(Size))
    {
        TArray<uint8> ReceivedData;
        ReceivedData.SetNumUninitialized(FMath::Min(Size, 65507u));

        int32 BytesRead = 0;
        TSharedRef<FInternetAddr> FromAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        ListenSocket->RecvFrom(ReceivedData.GetData(), ReceivedData.Num(), BytesRead, *FromAddress);

        if (BytesRead > 0)
        {
            ReceivedData.Add(0);
            FString JsonString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char *>(ReceivedData.GetData())));

            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

            if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
            {
                FString TypeString;
                if (JsonObject->TryGetStringField(TEXT("type"), TypeString))
                {
                    if (TypeString == TEXT("highlight_on") || TypeString == TEXT("highlight_off"))
                    {
                        const TArray<TSharedPtr<FJsonValue>>* NotesJsonArray;
                        if (JsonObject->TryGetArrayField(TEXT("notes"), NotesJsonArray))
                        {
                            TArray<int32> Notes;
                            for (const TSharedPtr<FJsonValue>& Val : *NotesJsonArray)
                            {
                                Notes.Add(static_cast<int32>(Val->AsNumber()));
                            }

                            bool bIsHighlightOn = (TypeString == TEXT("highlight_on"));

                            if (PianoActorRef)
                            {
                                if (bIsHighlightOn)
                                {
                                    PianoActorRef->HighlightKeys(Notes);
                                }
                                else
                                {
                                    PianoActorRef->UnhighlightKeys(Notes);
                                }
                            }

                            if (OnMidiHighlightEvent.IsBound())
                            {
                                OnMidiHighlightEvent.Broadcast(Notes, bIsHighlightOn);
                            }
                        }
                    }
                    else
                    {
                        int32 noteNumber = -1;
                        JsonObject->TryGetNumberField(TEXT("note"), noteNumber);

                        double duration = 0.0;
                        JsonObject->TryGetNumberField(TEXT("duration"), duration);

                        FString source = TEXT("live");
                        JsonObject->TryGetStringField(TEXT("source"), source);

                        bool isNoteOn = (TypeString == TEXT("note_on"));

                        if (OnMidiNoteEvent.IsBound())
                        {
                            OnMidiNoteEvent.Broadcast(noteNumber, isNoteOn, static_cast<float>(duration), source);
                        }

                        if (PianoActorRef)
                        {
                            PianoActorRef->HandleMidiEventWithSource(noteNumber, isNoteOn, source);
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to parse JSON: %s"), *JsonString);
            }
        }
    }
}

void AUDPMidiReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("UDP Receiver: Socket closed."));
    }
}