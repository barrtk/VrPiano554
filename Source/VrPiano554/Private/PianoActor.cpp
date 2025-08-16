// PianoActor.cpp

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

    // Create and configure the debug cylinder
    DebugCylinder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugCalibrationCylinder"));
    DebugCylinder->SetupAttachment(RootComponent);
    DebugCylinder->SetVisibility(true); // Make it visible by default for debugging
    DebugCylinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DebugCylinder->SetRelativeLocation(FVector(50.f, 0.f, 0.f)); // Move it in front of the piano model

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderAsset.Succeeded())
    {
        DebugCylinder->SetStaticMesh(CylinderAsset.Object);
        DebugCylinder->SetRelativeScale3D(FVector(0.05f, 0.05f, 10.0f));
    }

    static ConstructorHelpers::FObjectFinder<UMaterial> MaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (MaterialAsset.Succeeded())
    {
        DebugCylinder->SetMaterial(0, MaterialAsset.Object);
    }

    for (int32 i = 36; i <= 96; ++i)
    {
        FName ComponentName = FName(*FString::Printf(TEXT("Note%d"), i));
        UStaticMeshComponent* KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
        KeyMesh->SetupAttachment(RootComponent);
        KeyMeshComponents.Add(i, KeyMesh);
    }
}

void APianoActor::BeginPlay()
{
    Super::BeginPlay();

    // Defer controller setup to give the Pawn time to spawn.
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, this, &APianoActor::SetupControllers, 1.0f, false);

    EnableInput(GetWorld()->GetFirstPlayerController());
    if (InputComponent)
    {
        InputComponent->BindAction("StartKalibracji", IE_Pressed, this, &APianoActor::StartCalibration);
        InputComponent->BindAction("UstawLewyPunkt", IE_Pressed, this, &APianoActor::SetLeftCalibrationPoint);
        InputComponent->BindAction("UstawPrawyPunkt", IE_Pressed, this, &APianoActor::SetRightCalibrationPoint);
    }

    // Set initial materials for keys
    const TSet<int32> BlackKeyIndexes = {1, 3, 6, 8, 10};
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
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("PianoActor: Hierarchia przebudowana w kodzie."));
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

    // --- DEBUG CYLINDER ---
    // The cylinder is now placed at the target midpoint. The piano's visual center should align with it.
    if (DebugCylinder)
    {
        DebugCylinder->SetWorldLocation(MidPoint);
        DebugCylinder->SetWorldRotation(NewRotation);
        DebugCylinder->SetVisibility(true);
    }
    // --- END DEBUG ---

    float Distance = FVector::Dist(LeftCalibrationTransform.GetLocation(), RightCalibrationTransform.GetLocation());
    float NewScale = Distance / PianoModelWidth;

    SetActorLocationAndRotation(NewLocation, NewRotation);
    SetActorScale3D(FVector(NewScale));

    CalibrationState = ECalibrationState::Idle;
    UKismetSystemLibrary::PrintString(this, TEXT("Kalibracja zakończona!"), true, true, FLinearColor::Green, 10.f);
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
