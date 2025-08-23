#include "PianoActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Containers/Set.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "MotionControllerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/WidgetComponent.h"
#include "PianoMenuWidget.h" // Required for UPianoMenuWidget
#include "Sockets.h" // Added for FSocket
#include "SocketSubsystem.h" // Added for ISocketSubsystem
#include "Interfaces/IPv4/IPv4Address.h" // Added for FIPv4Address
#include "Common/UdpSocketBuilder.h" // Added for FUdpSocketBuilder
#include "PianoSaveGame.h" // Added for UPianoSaveGame

APianoActor::APianoActor()
{
    PrimaryActorTick.bCanEverTick = true;

    StableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StableRoot"));
    RootComponent = StableRoot;

    Coffre = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("coffre"));
    Coffre->SetupAttachment(RootComponent);
    Potards = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("potards"));
    Potards->SetupAttachment(RootComponent);
    PotardCentral = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("potard_central"));
    PotardCentral->SetupAttachment(RootComponent);
    Pads = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("pads"));
    Pads->SetupAttachment(RootComponent);
    Molettes = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("molettes"));
    Molettes->SetupAttachment(RootComponent);
    LCD = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LCD"));
    LCD->SetupAttachment(RootComponent);
    CurseursNew = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("curseurs_new"));
    CurseursNew->SetupAttachment(RootComponent);
    Connectique = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("connectique"));
    Connectique->SetupAttachment(RootComponent);

    for (int32 i = 36; i <= 96; ++i)
    {
        FName ComponentName = FName(*FString::Printf(TEXT("Note%d"), i));
        UStaticMeshComponent* KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
        KeyMesh->SetupAttachment(RootComponent);
        KeyMeshComponents.Add(i, KeyMesh);
    }

    // Create and configure the menu widget component
    MenuWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MenuWidget"));
    MenuWidgetComponent->SetupAttachment(RootComponent);
    MenuWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    MenuWidgetComponent->SetDrawSize(FVector2D(500, 300));
    MenuWidgetComponent->SetVisibility(true);
    MenuWidgetComponent->SetRelativeLocation(FVector(0, 0, 150.f)); // Position it behind the piano

    static ConstructorHelpers::FClassFinder<UUserWidget> MenuWidgetClass(TEXT("/Game/WBP_PianoMenu"));
    if (MenuWidgetClass.Succeeded())
    {
        MenuWidgetComponent->SetWidgetClass(MenuWidgetClass.Class);
    }

    // Widget Interaction (child of root for now, will re-attach to RightController in BeginPlay)
    WidgetInteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
    WidgetInteractionComponent->SetupAttachment(RootComponent); // Attach to RootComponent initially
    WidgetInteractionComponent->bShowDebug = true;
    WidgetInteractionComponent->InteractionDistance = 1000.0f;
    WidgetInteractionComponent->InteractionSource = EWidgetInteractionSource::World;
    WidgetInteractionComponent->PointerIndex = 0;
    WidgetInteractionComponent->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));

    bIsPaused = false;
    bIsLearningMode = false;
    bIsFileMuted = false;
    bIsLiveMuted = false;
    bIsLifeHoldActive = false;

    SenderSocket = nullptr; // Initialize socket pointer
}

void APianoActor::BeginPlay()
{
    Super::BeginPlay();

    if (MenuWidgetComponent)
    {
        MenuWidgetComponent->SetVisibility(false);
        PianoMenuWidgetInstance = Cast<UPianoMenuWidget>(MenuWidgetComponent->GetUserWidgetObject());
    }

    // Defer controller setup to give the Pawn time to spawn.
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, this, &APianoActor::SetupControllers, 1.0f, false);

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController(); // Get PlayerController
    EnableInput(PlayerController); // Pass PlayerController to EnableInput

    if (InputComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("APianoActor: InputComponent is valid. Binding actions."));
        InputComponent->BindAction("StartKalibracji", IE_Pressed, this, &APianoActor::StartCalibration);
        InputComponent->BindAction("UstawLewyPunkt", IE_Pressed, this, &APianoActor::SetLeftCalibrationPoint);
        InputComponent->BindAction("UstawPrawyPunkt", IE_Pressed, this, &APianoActor::SetRightCalibrationPoint);
        InputComponent->BindAction("ToggleMenu", IE_Pressed, this, &APianoActor::ToggleMenu);

        // Bind Trigger actions for widget interaction
        InputComponent->BindAction("TriggerRight", IE_Pressed, this, &APianoActor::OnRightTriggerPressed);
        InputComponent->BindAction("TriggerRight", IE_Released, this, &APianoActor::OnRightTriggerReleased);
        InputComponent->BindAction("TriggerLeft", IE_Pressed, this, &APianoActor::OnLeftTriggerPressed);
        InputComponent->BindAction("TriggerLeft", IE_Released, this, &APianoActor::OnLeftTriggerReleased);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("APianoActor: InputComponent is NULL! Cannot bind actions."));
    }

    // Set initial materials for keys
    const TSet<int32> BlackKeyIndexes = { 1, 3, 6, 8, 10 };
    for (const TPair<int32, UStaticMeshComponent*>& Pair : KeyMeshComponents)
    {
        if (UStaticMeshComponent* KeyComponent = Pair.Value)
        {
            if (BlackKeyIndexes.Contains(Pair.Key % 12))
            {
                if (BlackKeyMaterial)
                {
                    KeyComponent->SetMaterial(0, BlackKeyMaterial);
                }
            }
            else
            {
                if (WhiteKeyMaterial)
                {
                    KeyComponent->SetMaterial(0, WhiteKeyMaterial);
                }
            }
        }
    }

    KeyPivotMap.Empty();

    UE_LOG(LogTemp, Log, TEXT("PianoActor: Starting KeyPivotMap population loop."));

    for (const TPair<int32, UStaticMeshComponent*>& Pair : KeyMeshComponents)
    {
        int32 MidiNote = Pair.Key;
        UStaticMeshComponent* KeyComponent = Pair.Value;

        if (KeyComponent)
        {
            FBoxSphereBounds Bounds = KeyComponent->CalcBounds(KeyComponent->GetComponentTransform());

            // Direction to the back of the key (local -X)
            FVector LocalBackDirection = FVector::BackwardVector;
            FVector WorldBackDirection = KeyComponent->GetComponentTransform().TransformVectorNoScale(LocalBackDirection);

            // Direction to the TOP of the key (local +Z)
            FVector LocalUpDirection = FVector::UpVector;
            FVector WorldUpDirection = KeyComponent->GetComponentTransform().TransformVectorNoScale(LocalUpDirection);

            // Calculate pivot position: from center, move to back and to TOP
            FVector PivotWorldPosition = Bounds.Origin
                + (WorldBackDirection * Bounds.BoxExtent.X)
                + (WorldUpDirection * Bounds.BoxExtent.Z);

            FName PivotName = FName(*FString::Printf(TEXT("Pivot_%d"), MidiNote));
            USceneComponent* NewPivot = NewObject<USceneComponent>(this, PivotName);

            if (NewPivot)
            {
                NewPivot->SetupAttachment(RootComponent);
                NewPivot->SetWorldLocation(PivotWorldPosition);
                NewPivot->RegisterComponent();
                KeyPivotMap.Add(MidiNote, NewPivot);
                KeyComponent->AttachToComponent(NewPivot, FAttachmentTransformRules::KeepWorldTransform);
                UE_LOG(LogTemp, Log, TEXT("PianoActor: Added MidiNote %d to KeyPivotMap."), MidiNote);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("PianoActor: Failed to create NewPivot for MidiNote %d."), MidiNote);
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("PianoActor: KeyPivotMap populated with %d entries."), KeyPivotMap.Num());

    // Broadcast event that keys are initialized
    OnKeysInitialized.Broadcast();

    // Initialize UDP sender socket
    SenderSocket = FUdpSocketBuilder(TEXT("PianoActorSenderSocket"))
        .AsReusable()
        .WithBroadcast();

    if (!SenderSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("APianoActor: Failed to create UDP Sender Socket!"));
    }
}

void APianoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (SenderSocket)
    {
        SenderSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SenderSocket);
        SenderSocket = nullptr;
    }
}

void APianoActor::ToggleMenu()
{
    if (MenuWidgetComponent)
    {
        const bool bIsNowVisible = !MenuWidgetComponent->IsVisible();
        MenuWidgetComponent->SetVisibility(bIsNowVisible);
        OnMenuToggled.Broadcast(bIsNowVisible);
    }
}

void APianoActor::AdjustPositionX(float Value)
{
    AddActorWorldOffset(FVector(Value, 0.f, 0.f));
}

void APianoActor::AdjustPositionY(float Value)
{
    AddActorWorldOffset(FVector(0.f, Value, 0.f));
}

void APianoActor::AdjustPositionZ(float Value)
{
    AddActorWorldOffset(FVector(0.f, 0.f, Value));
}

void APianoActor::SetupControllers()
{
    LeftController = nullptr;
    RightController = nullptr;

    for (TObjectIterator<UMotionControllerComponent> It; It; ++It)
    {
        UMotionControllerComponent* MC = *It;
        if (MC->GetWorld() != GetWorld() || !MC->IsActive())
        {
            continue;
        }

        if (MC->GetTrackingSource() == EControllerHand::Left)
        {
            LeftController = MC;
        }
        else if (MC->GetTrackingSource() == EControllerHand::Right)
        {
            RightController = MC;
        }
    }

    if (!LeftController || !RightController)
    {
        UE_LOG(LogTemp, Error, TEXT("APianoActor: Could not find both Left and Right controllers using TObjectIterator. Retrying..."));
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, this, &APianoActor::SetupControllers, 1.0f, false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("APianoActor: Successfully found and assigned controllers using TObjectIterator."));
        // Attach WidgetInteractionComponent to RightController
        if (WidgetInteractionComponent && RightController)
        {
            WidgetInteractionComponent->AttachToComponent(RightController, FAttachmentTransformRules::KeepRelativeTransform);
            UE_LOG(LogTemp, Warning, TEXT("APianoActor: WidgetInteractionComponent attached to RightController."));
        }
    }
}

void APianoActor::StartCalibration()
{
    if (!LeftController || !RightController)
    {
        UKismetSystemLibrary::PrintString(this, TEXT("BŁĄD: Brak aktywnych kontrolerów VR!"), true, true, FLinearColor::Red, 10.f);
        return;
    }
    CalibrationState = ECalibrationState::WaitingForLeftPoint;
    UKismetSystemLibrary::PrintString(this, TEXT("Kalibracja: Ustaw lewy kontroler i wciśnij grip."), true, true, FLinearColor::Yellow, 10.f);
}

void APianoActor::SetLeftCalibrationPoint()
{
    if (CalibrationState == ECalibrationState::WaitingForLeftPoint)
    {
        LeftCalibrationTransform = LeftController->GetComponentTransform();
        CalibrationState = ECalibrationState::WaitingForRightPoint;
        UKismetSystemLibrary::PrintString(this, TEXT("Lewy punkt zapisany. Ustaw prawy kontroler i wciśnij grip."), true, true, FLinearColor::Yellow, 10.f);
    }
}

void APianoActor::SetRightCalibrationPoint()
{
    if (CalibrationState == ECalibrationState::WaitingForRightPoint)
    {
        RightCalibrationTransform = RightController->GetComponentTransform();
        UKismetSystemLibrary::PrintString(this, TEXT("Prawy punkt zapisany. Stosowanie kalibracji..."), true, true, FLinearColor::Green, 10.f);
        ApplyCalibration();

        // Pętla do automatycznego obniżenia pianina
        for (int i = 0; i < 10; ++i)
        {
            AdjustPositionZ(-1.0f);
        }
        UKismetSystemLibrary::PrintString(this, TEXT("Pianino zostało automatycznie obniżone."), true, true, FLinearColor::Yellow, 10.f);
    }
}

void APianoActor::ApplyCalibration()
{
    FVector MidPoint = FMath::Lerp(LeftCalibrationTransform.GetLocation(), RightCalibrationTransform.GetLocation(), 0.5f);
    FVector Direction = (RightCalibrationTransform.GetLocation() - LeftCalibrationTransform.GetLocation()).GetSafeNormal();
    FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();

    // Use the auto-calculated offset
    FVector RotatedOffset = NewRotation.RotateVector(CalculatedOffset);
    FVector NewLocation = MidPoint - RotatedOffset;

    float Distance = FVector::Dist(LeftCalibrationTransform.GetLocation(), RightCalibrationTransform.GetLocation());
    float NewScale = Distance / PianoModelWidth;

    SetActorLocationAndRotation(NewLocation, NewRotation);
    SetActorScale3D(FVector(NewScale));

    CalibrationState = ECalibrationState::Idle;
    UKismetSystemLibrary::PrintString(this, TEXT("Kalibracja zakończona!"), true, true, FLinearColor::Green, 10.f);

    OnCalibrationComplete.Broadcast();
}

void APianoActor::PressKey(int32 MidiNote)
{
    if (KeyPivotMap.Contains(MidiNote))
    {
        ActiveKeyAnimations.Add(MidiNote, TargetRotationAngle);
    }
}

void APianoActor::ReleaseKey(int32 MidiNote)
{
    if (KeyPivotMap.Contains(MidiNote))
    {
        ActiveKeyAnimations.Add(MidiNote, 0.0f);
    }
}

void APianoActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TMap<int32, float> AnimationsToProcess = ActiveKeyAnimations;
    for (const TPair<int32, float>& Pair : AnimationsToProcess)
    {
        int32 MidiNote = Pair.Key;
        float TargetPitch = Pair.Value;

        if (USceneComponent* Pivot = KeyPivotMap.FindRef(MidiNote))
        {
            FRotator CurrentRotation = Pivot->GetRelativeRotation();
            FRotator TargetRotator = FRotator(0.0f, 0.0f, TargetPitch);
            FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotator, DeltaTime, AnimationSpeed);
            Pivot->SetRelativeRotation(NewRotation);

            if (FMath::IsNearlyEqual(NewRotation.Roll, TargetRotator.Roll, 0.01f))
            {
                Pivot->SetRelativeRotation(TargetRotator);
                ActiveKeyAnimations.Remove(MidiNote);
            }
        }
    }
}

void APianoActor::HandleMidiNote(int32 Note, bool bIsNoteOn)
{
    if (bIsNoteOn)
    {
        PressKey(Note);
    }
    else
    {
        ReleaseKey(Note);
    }
}

void APianoActor::HandleMidiEventWithSource(int32 Note, bool bIsNoteOn, const FString& Source)
{
    if (Source.Equals(TEXT("file"), ESearchCase::IgnoreCase) && bIsFileAnimationMuted)
    {
        return; // Zignoruj zdarzenie, jeśli animacje z pliku są wyciszone
    }

    if (bIsNoteOn)
    {
        PressKey(Note);
    }
    else
    {
        ReleaseKey(Note);
    }
}

void APianoActor::HighlightKeys(const TArray<int32>& NotesToHighlight)
{
    if (!HighlightedKeyMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("PianoActor: HighlightedKeyMaterial is not set."));
        return;
    }

    for (int32 Note : NotesToHighlight)
    {
        if (UStaticMeshComponent** KeyComponentPtr = KeyMeshComponents.Find(Note))
        {
            if (UStaticMeshComponent* KeyComponent = *KeyComponentPtr)
            {
                // Store the original material if not already highlighted
                if (!OriginalKeyMaterials.Contains(Note))
                {
                    OriginalKeyMaterials.Add(Note, KeyComponent->GetMaterial(0));
                }
                // Set the highlight material
                KeyComponent->SetMaterial(0, HighlightedKeyMaterial);
            }
        }
    }
}

void APianoActor::UnhighlightKeys(const TArray<int32>& NotesToUnhighlight)
{
    for (int32 Note : NotesToUnhighlight)
    {
        if (UStaticMeshComponent** KeyComponentPtr = KeyMeshComponents.Find(Note))
        {
            if (UStaticMeshComponent* KeyComponent = *KeyComponentPtr)
            {
                // Restore the original material if it was stored
                if (UMaterialInterface** OriginalMaterial = OriginalKeyMaterials.Find(Note))
                {
                    KeyComponent->SetMaterial(0, *OriginalMaterial);
                    OriginalKeyMaterials.Remove(Note);
                }
            }
        }
    }
}

void APianoActor::OnRightTriggerPressed()
{
    if (WidgetInteractionComponent)
    {
        WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
    }
}

void APianoActor::OnRightTriggerReleased()
{
    if (WidgetInteractionComponent)
    {
        WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
    }
}

void APianoActor::OnLeftTriggerPressed()
{
    // Optional: If you want left trigger to also interact with widgets
    // if (WidgetInteractionComponent)
    // {
    //     WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
    //     UE_LOG(LogTemp, Warning, TEXT("APianoActor: Left Trigger Pressed - Simulating Left Mouse Button."));
    // }
}

void APianoActor::OnLeftTriggerReleased()
{
    // Optional: If you want left trigger to also interact with widgets
    // if (WidgetInteractionComponent)
    // {
    //     WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
    //     UE_LOG(LogTemp, Warning, TEXT("APianoActor: Left Trigger Released - Releasing Left Mouse Button."));
    // }
}

void APianoActor::SavePosition()
{
    if (UPianoSaveGame* SaveGameInstance = Cast<UPianoSaveGame>(UGameplayStatics::CreateSaveGameObject(UPianoSaveGame::StaticClass())))
    {
        SaveGameInstance->PianoTransform = GetActorTransform();
        UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveGameInstance->SaveSlotName, SaveGameInstance->UserIndex);
        if (PianoMenuWidgetInstance)
        {
            
        }
    }
}

void APianoActor::LoadPosition()
{
    if (UPianoSaveGame* LoadedGame = Cast<UPianoSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("PianoPositionSaveSlot"), 0)))
    {
        SetActorTransform(LoadedGame->PianoTransform);
        if (PianoMenuWidgetInstance)
        {
            
        }
    }
}

void APianoActor::ResetPosition()
{
    SetActorTransform(FTransform::Identity);
}

void APianoActor::SendUDPCommand(const FString& Command)
{
    if (!SenderSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("APianoActor: SenderSocket is not initialized!"));
        return;
    }

    FString JsonString = FString::Printf(TEXT("{\"command\":\"%s\"}"), *Command);
    TArray<uint8> Data;
    Data.Append((uint8*)TCHAR_TO_UTF8(*JsonString), JsonString.Len());

    FIPv4Address Addr;
    FIPv4Address::Parse(TEXT("127.0.0.1"), Addr);
    TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    InternetAddr->SetIp(Addr.Value);
    InternetAddr->SetPort(5009);

    int32 BytesSent = 0;
    SenderSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *InternetAddr);
}

void APianoActor::TogglePauseState()
{
    bIsPaused = !bIsPaused;
    OnPauseStateChanged.Broadcast(bIsPaused);
    SendUDPCommand(TEXT("pauza"));
}

void APianoActor::ToggleLearningMode()
{
    bIsLearningMode = !bIsLearningMode;
    OnLearningModeStateChanged.Broadcast(bIsLearningMode);

    if (!bIsLearningMode) // If exiting learning mode
    {
        // Unhighlight all currently highlighted keys
        TArray<int32> KeysToUnhighlight;
        OriginalKeyMaterials.GetKeys(KeysToUnhighlight);
        UnhighlightKeys(KeysToUnhighlight);
    }

    SendUDPCommand(TEXT("tryb_nauki"));
}

void APianoActor::ToggleFileMute()
{
    bIsFileMuted = !bIsFileMuted;
    OnFileMuteStateChanged.Broadcast(bIsFileMuted);
    SendUDPCommand(TEXT("mute_file"));
}

void APianoActor::ToggleLiveMute()
{
    bIsLiveMuted = !bIsLiveMuted;
    OnLiveMuteStateChanged.Broadcast(bIsLiveMuted);
    SendUDPCommand(TEXT("mute_live"));
}

void APianoActor::ToggleLifeHold()
{
    bIsLifeHoldActive = !bIsLifeHoldActive;
    OnLifeHoldStateChanged.Broadcast(bIsLifeHoldActive);
    SendUDPCommand(TEXT("life_hold"));
}

void APianoActor::MidiSlower()
{
    SendUDPCommand(TEXT("midi_wolniej"));
}

void APianoActor::MidiFaster()
{
    SendUDPCommand(TEXT("midi_szybciej"));
}

void APianoActor::PrevMidi()
{
    SendUDPCommand(TEXT("prev_midi"));
}

void APianoActor::NextMidi()
{
    SendUDPCommand(TEXT("next_midi"));
}

void APianoActor::UnmuteAll()
{
    SendUDPCommand(TEXT("unmute_all"));
}

void APianoActor::ToggleLoop()
{
    SendUDPCommand(TEXT("toggle_loop"));
}

void APianoActor::StartRestart()
{
    // If the file is muted, unmute it to match the default state
    // of the Python script after a restart.
    if (bIsFileMuted)
    {
        bIsFileMuted = false;
        OnFileMuteStateChanged.Broadcast(bIsFileMuted); // This will update the UI
    }

    SendUDPCommand(TEXT("start_restart"));
    UKismetSystemLibrary::PrintString(this, TEXT("Restarting MIDI and Application..."), true, true, FLinearColor::Blue, 10.f);
    if (PianoMenuWidgetInstance)
    {
        PianoMenuWidgetInstance->UpdateMidiText(TEXT("MIDI: Restarting..."));
    }
}

void APianoActor::ToggleFileAnimationMute()
{
    bIsFileAnimationMuted = !bIsFileAnimationMuted;
    OnFileAnimationMuteStateChanged.Broadcast(bIsFileAnimationMuted);

    // Jeśli animacje zostały właśnie wyłączone, zresetuj pozycję wszystkich klawiszy
    if (bIsFileAnimationMuted)
    {
        for (int32 Note = 36; Note <= 96; ++Note)
        {
            ReleaseKey(Note); // Użyj istniejącej funkcji, by przywrócić pozycję domyślną
        }
    }
}

void APianoActor::PlayNote(int32 MidiNote, float Duration)
{
    // Immediately press the key
    PressKey(MidiNote);

    // Set a timer to release the key after the specified duration
    FTimerHandle ReleaseTimerHandle;
    FTimerDelegate ReleaseDelegate;

    // Use a lambda to capture the MidiNote value
    ReleaseDelegate.BindLambda([this, MidiNote]()
    {
        ReleaseKey(MidiNote);
    });

    GetWorldTimerManager().SetTimer(ReleaseTimerHandle, ReleaseDelegate, Duration, false);
}

void APianoActor::LoadMidiFile()
{
    // Implement MIDI file loading logic if needed
}

    bool APianoActor::GetKeyTransformAndWidth(int32 MidiNote, FTransform& OutTransform, float& OutWidth)
    {
        UE_LOG(LogTemp, Log, TEXT("GetKeyTransformAndWidth: Checking MidiNote %d"), MidiNote);
        if (USceneComponent** PivotPtr = KeyPivotMap.Find(MidiNote))
        {
            UE_LOG(LogTemp, Log, TEXT("GetKeyTransformAndWidth: Found Pivot for MidiNote %d"), MidiNote);
            if (USceneComponent* Pivot = *PivotPtr)
            {
                OutTransform = Pivot->GetComponentTransform();

                // Calculate width from the attached mesh component
                if (UStaticMeshComponent** KeyMeshPtr = KeyMeshComponents.Find(MidiNote))
                {
                    UE_LOG(LogTemp, Log, TEXT("GetKeyTransformAndWidth: Found KeyMesh for MidiNote %d"), MidiNote);
                    if (UStaticMeshComponent* KeyMesh = *KeyMeshPtr)
                    {
                        // Get the local bounds of the mesh
                        FBoxSphereBounds LocalBounds = KeyMesh->GetStaticMesh()->GetBounds();
                        // The width is typically along the Y-axis in a standard piano key mesh
                        // Assuming the mesh is oriented such that its Y-axis represents width
                        OutWidth = LocalBounds.BoxExtent.Y * 2.0f * KeyMesh->GetComponentScale().Y;
                        return true;
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("GetKeyTransformAndWidth: KeyMeshComponents.Find failed for MidiNote %d"), MidiNote);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("GetKeyTransformAndWidth: KeyPivotMap.Find failed for MidiNote %d"), MidiNote);
        }
        OutTransform = FTransform::Identity;
        OutWidth = 0.0f;
        return false;
    }

void APianoActor::BroadcastKeysInitialized()
{
    OnKeysInitialized.Broadcast();
}

FString APianoActor::GetNoteName(int32 MidiNote)
{
    static const FString NoteNames[] = {
        TEXT("C"), TEXT("C#"), TEXT("D"), TEXT("D#"), TEXT("E"), TEXT("F"),
        TEXT("F#"), TEXT("G"), TEXT("G#"), TEXT("A"), TEXT("A#"), TEXT("B")
    };

    int32 NoteIndex = MidiNote % 12;
    int32 Octave = (MidiNote / 12) - 1; // MIDI note 0 is C-1

    return FString::Printf(TEXT("%s%d"), *NoteNames[NoteIndex], Octave);
}
