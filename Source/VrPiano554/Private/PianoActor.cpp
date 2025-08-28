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

    MenuWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MenuWidget"));
    MenuWidgetComponent->SetupAttachment(RootComponent);
    MenuWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    MenuWidgetComponent->SetDrawSize(FVector2D(500, 300));
    MenuWidgetComponent->SetVisibility(true);
    MenuWidgetComponent->SetRelativeLocation(FVector(0, 0, 150.f));

    static ConstructorHelpers::FClassFinder<UUserWidget> MenuWidgetClass(TEXT("/Game/WBP_PianoMenu"));
    if (MenuWidgetClass.Succeeded())
    {
        MenuWidgetComponent->SetWidgetClass(MenuWidgetClass.Class);
    }

    WidgetInteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
    WidgetInteractionComponent->SetupAttachment(RootComponent);
    WidgetInteractionComponent->bShowDebug = true;
    WidgetInteractionComponent->InteractionDistance = 1000.0f;
    WidgetInteractionComponent->InteractionSource = EWidgetInteractionSource::World;
    WidgetInteractionComponent->PointerIndex = 0;
    WidgetInteractionComponent->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));

    bIsPaused = false;
    bIsLearningMode = true;
    bIsFileMuted = false;
    bIsLiveMuted = false;
    bIsLifeHoldActive = false;

    SenderSocket = nullptr;
	CalculatedOffset = FVector::ZeroVector;
}

void APianoActor::BeginPlay()
{
    Super::BeginPlay();

    if (MenuWidgetComponent)
    {
        MenuWidgetComponent->SetVisibility(false);
        PianoMenuWidgetInstance = Cast<UPianoMenuWidget>(MenuWidgetComponent->GetUserWidgetObject());
    }

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, this, &APianoActor::SetupControllers, 1.0f, false);

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    EnableInput(PlayerController);

    if (InputComponent)
    {
        InputComponent->BindAction("StartKalibracji", IE_Pressed, this, &APianoActor::StartCalibration);
        InputComponent->BindAction("UstawLewyPunkt", IE_Pressed, this, &APianoActor::SetLeftCalibrationPoint);
        InputComponent->BindAction("UstawPrawyPunkt", IE_Pressed, this, &APianoActor::SetRightCalibrationPoint);
        InputComponent->BindAction("ToggleMenu", IE_Pressed, this, &APianoActor::ToggleMenu);
        InputComponent->BindAction("TriggerRight", IE_Pressed, this, &APianoActor::OnRightTriggerPressed);
        InputComponent->BindAction("TriggerRight", IE_Released, this, &APianoActor::OnRightTriggerReleased);
        InputComponent->BindAction("TriggerLeft", IE_Pressed, this, &APianoActor::OnLeftTriggerPressed);
        InputComponent->BindAction("TriggerLeft", IE_Released, this, &APianoActor::OnLeftTriggerReleased);
    }

    const TSet<int32> BlackKeyIndexes = { 1, 3, 6, 8, 10 };
    for (const TPair<int32, UStaticMeshComponent*>& Pair : KeyMeshComponents)
    {
        if (UStaticMeshComponent* KeyComponent = Pair.Value)
        {
            if (BlackKeyIndexes.Contains(Pair.Key % 12))
            {
                if (BlackKeyMaterial) KeyComponent->SetMaterial(0, BlackKeyMaterial);
            }
            else
            {
                if (WhiteKeyMaterial) KeyComponent->SetMaterial(0, WhiteKeyMaterial);
            }
        }
    }

    KeyPivotMap.Empty();
    UE_LOG(LogTemp, Log, TEXT("PianoActor: Starting KeyPivotMap population loop."));

    for (const TPair<int32, UStaticMeshComponent*>& Pair : KeyMeshComponents)
    {
        int32 MidiNote = Pair.Key;
        UStaticMeshComponent* KeyComponent = Pair.Value;

        if (!IsValid(KeyComponent) || !IsValid(KeyComponent->GetStaticMesh()))
        {
            UE_LOG(LogTemp, Warning, TEXT("Key %d: Component or StaticMesh is invalid."), MidiNote);
            continue;
        }

        FBoxSphereBounds Bounds = KeyComponent->CalcBounds(KeyComponent->GetComponentTransform());
        if (Bounds.BoxExtent.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("Key %d: Mesh has zero bounds."), MidiNote);
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("Key %d: Bounds.Origin at X=%.2f, Y=%.2f, Z=%.2f"), MidiNote, Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z);

        FVector PivotWorldPosition = Bounds.Origin;

        FName PivotName = FName(*FString::Printf(TEXT("Pivot_%d"), MidiNote));
        USceneComponent* NewPivot = NewObject<USceneComponent>(this, PivotName);

        if (NewPivot)
        {
            NewPivot->SetupAttachment(RootComponent);
            NewPivot->SetWorldLocation(PivotWorldPosition);
            NewPivot->RegisterComponent();
            KeyPivotMap.Add(MidiNote, NewPivot);
            KeyComponent->AttachToComponent(NewPivot, FAttachmentTransformRules::KeepWorldTransform);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Key %d: Failed to create NewPivot object."), MidiNote);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("PianoActor: KeyPivotMap populated with %d entries."), KeyPivotMap.Num());

    OnKeysInitialized.Broadcast();

    SenderSocket = FUdpSocketBuilder(TEXT("PianoActorSenderSocket")).AsReusable().WithBroadcast();
    if (!SenderSocket) UE_LOG(LogTemp, Error, TEXT("APianoActor: Failed to create UDP Sender Socket!"));
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

void APianoActor::AdjustPositionX(float Value) { AddActorWorldOffset(FVector(Value, 0.f, 0.f)); }
void APianoActor::AdjustPositionY(float Value) { AddActorWorldOffset(FVector(0.f, Value, 0.f)); }
void APianoActor::AdjustPositionZ(float Value) { AddActorWorldOffset(FVector(0.f, 0.f, Value)); }

void APianoActor::SetupControllers()
{
    for (TObjectIterator<UMotionControllerComponent> It; It; ++It)
    {
        UMotionControllerComponent* MC = *It;
        if (MC->GetWorld() != GetWorld() || !MC->IsActive()) continue;
        if (MC->GetTrackingSource() == EControllerHand::Left) LeftController = MC;
        else if (MC->GetTrackingSource() == EControllerHand::Right) RightController = MC;
    }

    if (WidgetInteractionComponent && RightController)
    {
        WidgetInteractionComponent->AttachToComponent(RightController, FAttachmentTransformRules::KeepRelativeTransform);
    }
}

void APianoActor::StartCalibration()
{
    if (!LeftController || !RightController) 
    {
        UE_LOG(LogTemp, Warning, TEXT("APianoActor::StartCalibration - Controllers not found!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("APianoActor::StartCalibration - Calibration started, waiting for left point."));
    CalibrationState = ECalibrationState::WaitingForLeftPoint;
}

void APianoActor::SetLeftCalibrationPoint()
{
    if (CalibrationState == ECalibrationState::WaitingForLeftPoint)
    {
        LeftCalibrationTransform = LeftController->GetComponentTransform();
        CalibrationState = ECalibrationState::WaitingForRightPoint;
        UE_LOG(LogTemp, Log, TEXT("APianoActor::SetLeftCalibrationPoint - Left point set at %s. Waiting for right point."), *LeftCalibrationTransform.GetLocation().ToString());
    }
}

void APianoActor::SetRightCalibrationPoint()
{
    if (CalibrationState == ECalibrationState::WaitingForRightPoint)
    {
        RightCalibrationTransform = RightController->GetComponentTransform();
        UE_LOG(LogTemp, Log, TEXT("APianoActor::SetRightCalibrationPoint - Right point set at %s. Applying calibration."), *RightCalibrationTransform.GetLocation().ToString());
        ApplyCalibration();
    }
}

void APianoActor::ApplyCalibration()
{
    FVector LeftLocation = LeftCalibrationTransform.GetLocation();
    FVector RightLocation = RightCalibrationTransform.GetLocation();
    float Distance = FVector::Dist(LeftLocation, RightLocation);

    UE_LOG(LogTemp, Log, TEXT("APianoActor::ApplyCalibration - Left: %s, Right: %s, Distance: %f, PianoModelWidth: %f"), *LeftLocation.ToString(), *RightLocation.ToString(), Distance, PianoModelWidth);

    if (PianoModelWidth <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("APianoActor::ApplyCalibration - PianoModelWidth is zero or negative. Aborting calibration."));
        return;
    }

    FVector MidPoint = FMath::Lerp(LeftCalibrationTransform.GetLocation(), RightCalibrationTransform.GetLocation(), 0.5f);
    FVector Direction = (RightCalibrationTransform.GetLocation() - LeftCalibrationTransform.GetLocation()).GetSafeNormal();
    FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
    FVector RotatedOffset = NewRotation.RotateVector(CalculatedOffset);
    FVector NewLocation = MidPoint - RotatedOffset;
    NewLocation.Z -= 10.0f; // Apply the requested Z offset
    float NewScale = Distance / PianoModelWidth;

    UE_LOG(LogTemp, Log, TEXT("APianoActor::ApplyCalibration - New Location: %s, New Rotation: %s, New Scale: %f"), *NewLocation.ToString(), *NewRotation.ToString(), NewScale);

    SetActorLocationAndRotation(NewLocation, NewRotation);
    SetActorScale3D(FVector(NewScale));
    CalibrationState = ECalibrationState::Idle;
    OnCalibrationComplete.Broadcast();
}

void APianoActor::PressKey(int32 MidiNote)
{
    if (KeyPivotMap.Contains(MidiNote))
    {
        ActiveKeyAnimations.Add(MidiNote, TargetRotationAngle);
        OnPlayerNotePlayed.Broadcast(MidiNote);
    }
}

void APianoActor::ReleaseKey(int32 MidiNote)
{
    if (KeyPivotMap.Contains(MidiNote)) ActiveKeyAnimations.Add(MidiNote, 0.0f);
}

void APianoActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    TMap<int32, float> AnimationsToProcess = ActiveKeyAnimations;
    for (const TPair<int32, float>& Pair : AnimationsToProcess)
    {
        if (USceneComponent* Pivot = KeyPivotMap.FindRef(Pair.Key))
        {
            FRotator TargetRotator = FRotator(0.0f, 0.0f, Pair.Value);
            FRotator NewRotation = FMath::RInterpTo(Pivot->GetRelativeRotation(), TargetRotator, DeltaTime, AnimationSpeed);
            Pivot->SetRelativeRotation(NewRotation);
            if (FMath::IsNearlyEqual(NewRotation.Roll, TargetRotator.Roll, 0.01f)) ActiveKeyAnimations.Remove(Pair.Key);
        }
    }
}

void APianoActor::HandleMidiEventWithSource(int32 Note, bool bIsNoteOn, const FString& Source)
{
    if (Source.Equals(TEXT("file"), ESearchCase::IgnoreCase) && bIsFileAnimationMuted) return;
    bIsNoteOn ? PressKey(Note) : ReleaseKey(Note);
}

void APianoActor::HandleMidiNote(int32 Note, bool bIsNoteOn)
{
    // This function can be connected to a Blueprint event to handle MIDI notes.
    // For now, it just calls PressKey or ReleaseKey directly.
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
    if (!HighlightedKeyMaterial) return;
    for (int32 Note : NotesToHighlight)
    {
        if (UStaticMeshComponent* KeyComponent = KeyMeshComponents.FindRef(Note))
        {
            if (!OriginalKeyMaterials.Contains(Note)) OriginalKeyMaterials.Add(Note, KeyComponent->GetMaterial(0));
            KeyComponent->SetMaterial(0, HighlightedKeyMaterial);
        }
    }
}

void APianoActor::UnhighlightKeys(const TArray<int32>& NotesToUnhighlight)
{
    for (int32 Note : NotesToUnhighlight)
    {
        if (UMaterialInterface** OriginalMaterial = OriginalKeyMaterials.Find(Note))
        {
            if (UStaticMeshComponent* KeyComponent = KeyMeshComponents.FindRef(Note)) KeyComponent->SetMaterial(0, *OriginalMaterial);
            OriginalKeyMaterials.Remove(Note);
        }
    }
}

void APianoActor::HighlightKeyForDuration(int32 MidiNote, float Duration)
{
    if (FTimerHandle* ExistingTimer = KeyHighlightTimers.Find(MidiNote)) GetWorldTimerManager().ClearTimer(*ExistingTimer);
    HighlightKeys({MidiNote});
    FTimerHandle NewTimerHandle;
    FTimerDelegate UnhighlightDelegate;
    UnhighlightDelegate.BindLambda([this, MidiNote]() { UnhighlightKeys({MidiNote}); KeyHighlightTimers.Remove(MidiNote); });
    GetWorldTimerManager().SetTimer(NewTimerHandle, UnhighlightDelegate, Duration, false);
    KeyHighlightTimers.Add(MidiNote, NewTimerHandle);
}

void APianoActor::OnRightTriggerPressed() { if (WidgetInteractionComponent) WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton); }
void APianoActor::OnRightTriggerReleased() { if (WidgetInteractionComponent) WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton); }
void APianoActor::OnLeftTriggerPressed() {}
void APianoActor::OnLeftTriggerReleased() {}

void APianoActor::SavePosition()
{
    if (UPianoSaveGame* SaveGameInstance = Cast<UPianoSaveGame>(UGameplayStatics::CreateSaveGameObject(UPianoSaveGame::StaticClass())))
    {
        SaveGameInstance->PianoTransform = GetActorTransform();
        UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveGameInstance->SaveSlotName, SaveGameInstance->UserIndex);
    }
}

void APianoActor::LoadPosition()
{
    if (UPianoSaveGame* LoadedGame = Cast<UPianoSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("PianoPositionSaveSlot"), 0)))
    {
        SetActorTransform(LoadedGame->PianoTransform);
    }
}

void APianoActor::ResetPosition() { SetActorTransform(FTransform::Identity); }

void APianoActor::SendUDPCommand(const FString& Command)
{
    if (!SenderSocket) return;
    FString JsonString = FString::Printf(TEXT("{\"command\":\"%s\"}"), *Command);
    TArray<uint8> Data;
    Data.Append((uint8*)TCHAR_TO_UTF8(*JsonString), JsonString.Len());
    TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    InternetAddr->SetIp(FIPv4Address(127, 0, 0, 1).Value);
    InternetAddr->SetPort(5009);
    int32 BytesSent = 0;
    SenderSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *InternetAddr);
}

void APianoActor::TogglePauseState() { bIsPaused = !bIsPaused; OnPauseStateChanged.Broadcast(bIsPaused); SendUDPCommand(TEXT("pauza")); }
void APianoActor::ToggleLearningMode() { bIsLearningMode = !bIsLearningMode; OnLearningModeStateChanged.Broadcast(bIsLearningMode); SendUDPCommand(TEXT("tryb_nauki")); }
void APianoActor::ToggleFileMute() { bIsFileMuted = !bIsFileMuted; OnFileMuteStateChanged.Broadcast(bIsFileMuted); SendUDPCommand(TEXT("mute_file")); }
void APianoActor::ToggleLiveMute() { bIsLiveMuted = !bIsLiveMuted; OnLiveMuteStateChanged.Broadcast(bIsLiveMuted); SendUDPCommand(TEXT("mute_live")); }
void APianoActor::ToggleLifeHold() { bIsLifeHoldActive = !bIsLifeHoldActive; OnLifeHoldStateChanged.Broadcast(bIsLifeHoldActive); SendUDPCommand(TEXT("life_hold")); }
void APianoActor::MidiSlower() { SendUDPCommand(TEXT("midi_wolniej")); }
void APianoActor::MidiFaster() { SendUDPCommand(TEXT("midi_szybciej")); }
void APianoActor::PrevMidi() { SendUDPCommand(TEXT("prev_midi")); }
void APianoActor::NextMidi() { SendUDPCommand(TEXT("next_midi")); }
void APianoActor::UnmuteAll() { SendUDPCommand(TEXT("unmute_all")); }
void APianoActor::ToggleLoop() { SendUDPCommand(TEXT("toggle_loop")); }
void APianoActor::StartRestart() { SendUDPCommand(TEXT("start_restart")); }
void APianoActor::ToggleFileAnimationMute() { bIsFileAnimationMuted = !bIsFileAnimationMuted; OnFileAnimationMuteStateChanged.Broadcast(bIsFileAnimationMuted); }

void APianoActor::PlayNote(int32 MidiNote, float Duration)
{
    PressKey(MidiNote);
    FTimerHandle ReleaseTimerHandle;
    FTimerDelegate ReleaseDelegate;
    ReleaseDelegate.BindLambda([this, MidiNote]() { ReleaseKey(MidiNote); });
    GetWorldTimerManager().SetTimer(ReleaseTimerHandle, ReleaseDelegate, Duration, false);
}

void APianoActor::LoadMidiFile() {}

bool APianoActor::GetKeyTransformAndWidth(int32 MidiNote, FTransform& OutTransform, float& OutWidth)
{
    if (USceneComponent** PivotPtr = KeyPivotMap.Find(MidiNote))
    {
        if (USceneComponent* Pivot = *PivotPtr)
        {
            OutTransform = Pivot->GetComponentTransform();
            UE_LOG(LogTemp, Log, TEXT("GetKeyTransformAndWidth: Found transform for key %d at location X=%.2f, Y=%.2f, Z=%.2f"), MidiNote, OutTransform.GetLocation().X, OutTransform.GetLocation().Y, OutTransform.GetLocation().Z);
            if (UStaticMeshComponent** KeyMeshPtr = KeyMeshComponents.Find(MidiNote))
            {
                if (UStaticMeshComponent* KeyMesh = *KeyMeshPtr)
                {
                    FBoxSphereBounds LocalBounds = KeyMesh->GetStaticMesh()->GetBounds();
                    OutWidth = LocalBounds.BoxExtent.Y * 2.0f * KeyMesh->GetComponentScale().Y;
                    return true;
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GetKeyTransformAndWidth: Could not find pivot in KeyPivotMap for note %d."), MidiNote);
    }
    OutTransform = FTransform::Identity;
    OutWidth = 0.0f;
    return false;
}

void APianoActor::BroadcastKeysInitialized() { OnKeysInitialized.Broadcast(); }

FString APianoActor::GetNoteName(int32 MidiNote)
{
    static const FString NoteNames[] = { TEXT("C"), TEXT("C#"), TEXT("D"), TEXT("D#"), TEXT("E"), TEXT("F"), TEXT("F#"), TEXT("G"), TEXT("G#"), TEXT("A"), TEXT("A#"), TEXT("B") };
    int32 NoteIndex = MidiNote % 12;
    int32 Octave = (MidiNote / 12) - 1;
    return FString::Printf(TEXT("%s%d"), *NoteNames[NoteIndex], Octave);
}