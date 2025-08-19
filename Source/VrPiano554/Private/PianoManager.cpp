// PianoManager.cpp

#include "PianoManager.h"
#include "Kismet/GameplayStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PianoSaveGame.h"

APianoManager::APianoManager()
{
    PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void APianoManager::BeginPlay()
{
    Super::BeginPlay();
    SetupInput();
	LoadPianoTransform();
}

void APianoManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void APianoManager::SetupInput()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (PlayerController)
    {
        UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
        if (Subsystem)
        {
            // NOTE: You will need to create these assets in the editor.
            // 1. Create Input Action "IA_SpawnPiano"
            // 2. Create Input Action "IA_ScalePiano" (Value1D Axis)
            // 3. Create Input Mapping Context "IMC_PianoControls"
            // 4. In IMC_PianoControls, map IA_SpawnPiano to Right Controller 'A' button.
            // 5. In IMC_PianoControls, map IA_ScalePiano to Right Controller Thumbstick Y-axis.
            
            UInputMappingContext* PianoMappingContext = NewObject<UInputMappingContext>(this);
            
            UInputAction* SpawnAction = NewObject<UInputAction>(this);
            SpawnAction->ValueType = EInputActionValueType::Boolean;
            PianoMappingContext->MapKey(SpawnAction, EKeys::OculusTouch_Right_A_Click);

            UInputAction* ScaleAction = NewObject<UInputAction>(this);
            ScaleAction->ValueType = EInputActionValueType::Axis1D;
            PianoMappingContext->MapKey(ScaleAction, EKeys::OculusTouch_Right_Thumbstick_Y);

            Subsystem->AddMappingContext(PianoMappingContext, 0);

            UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
            if (EnhancedInputComponent)
            {
                EnhancedInputComponent->BindAction(SpawnAction, ETriggerEvent::Triggered, this, &APianoManager::SpawnOrResetPiano);
                EnhancedInputComponent->BindAction(ScaleAction, ETriggerEvent::Triggered, this, &APianoManager::ScalePiano);
            }
        }
    }
}

void APianoManager::SpawnOrResetPiano()
{
    UE_LOG(LogTemp, Warning, TEXT("SpawnOrResetPiano button pressed"));
    if (SpawnedPiano)
    {
        // If piano already exists, reset its transform
        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        if (PlayerPawn)
        {
            FVector Location = PlayerPawn->GetActorLocation() + (PlayerPawn->GetActorForwardVector() * 100.0f); // Place 1m in front of player
            FRotator Rotation = PlayerPawn->GetActorRotation();
            SpawnedPiano->SetActorTransform(FTransform(Rotation, Location, FVector::OneVector));
			UE_LOG(LogTemp, Log, TEXT("Piano transform reset"));
        }
    }
    else
    {
        // If piano doesn't exist, spawn it
        if (PianoActorClass)
        {
            APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
            if (PlayerPawn)
            {
                FVector Location = PlayerPawn->GetActorLocation() + (PlayerPawn->GetActorForwardVector() * 100.0f);
                FRotator Rotation = PlayerPawn->GetActorRotation();
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                SpawnedPiano = GetWorld()->SpawnActor<APianoActor>(PianoActorClass, Location, Rotation, SpawnParams);

				if (SpawnedPiano)
				{
					UE_LOG(LogTemp, Warning, TEXT("Piano spawned successfully."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn piano!"));
				}
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("PianoActorClass is not set in BP_PianoManager!"));
        }
    }
	SavePianoTransform();
}

void APianoManager::ScalePiano(const FInputActionValue& Value)
{
    if (SpawnedPiano)
    {
		float AxisValue = Value.Get<float>();
        UE_LOG(LogTemp, Warning, TEXT("ScalePiano axis value: %f"), AxisValue);
        FVector CurrentScale = SpawnedPiano->GetActorScale3D();
        float ScaleChange = AxisValue * ScaleSpeed * GetWorld()->GetDeltaSeconds();
        FVector NewScale = CurrentScale + FVector(ScaleChange);
        if (NewScale.X > 0.1f && NewScale.Y > 0.1f && NewScale.Z > 0.1f) // Prevent scaling to zero or negative
        {
            SpawnedPiano->SetActorScale3D(NewScale);
        }
    }
}

void APianoManager::SavePianoTransform()
{
    if (SpawnedPiano)
    {
        UPianoSaveGame* SaveGameInstance = Cast<UPianoSaveGame>(UGameplayStatics::CreateSaveGameObject(UPianoSaveGame::StaticClass()));
        if (SaveGameInstance)
        {
            SaveGameInstance->PianoTransform = SpawnedPiano->GetActorTransform();
            if (UGameplayStatics::SaveGameToSlot(SaveGameInstance, TEXT("PianoTransformSlot"), 0))
            {
                UE_LOG(LogTemp, Log, TEXT("Piano transform saved."));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to save piano transform."));
            }
        }
    }
}

void APianoManager::LoadPianoTransform()
{
    if (UGameplayStatics::DoesSaveGameExist(TEXT("PianoTransformSlot"), 0))
    {
        UPianoSaveGame* LoadGameInstance = Cast<UPianoSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("PianoTransformSlot"), 0));
        if (LoadGameInstance)
        {
            if (PianoActorClass)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                SpawnedPiano = GetWorld()->SpawnActor<APianoActor>(PianoActorClass, LoadGameInstance->PianoTransform.GetLocation(), LoadGameInstance->PianoTransform.GetRotation().Rotator(), SpawnParams);

				if (SpawnedPiano)
				{
					UE_LOG(LogTemp, Warning, TEXT("Piano loaded and spawned successfully."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn piano from save data!"));
				}
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("No saved piano data found."));
    }
}