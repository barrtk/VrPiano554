#include "VrPianoPawn.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// Sets default values
AVrPianoPawn::AVrPianoPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root for VR tracking
	VRTrackingCenter = CreateDefaultSubobject<USceneComponent>(TEXT("VRTrackingCenter"));
	RootComponent = VRTrackingCenter;

	// Camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(VRTrackingCenter);

	// Left controller
	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MC_Left"));
	LeftController->SetupAttachment(VRTrackingCenter);
	LeftController->SetTrackingMotionSource(FName("Left"));

	// Right controller
	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MC_Right"));
	RightController->SetupAttachment(VRTrackingCenter);
	RightController->SetTrackingMotionSource(FName("Right"));

	// Widget Interaction (child of right controller)
	WidgetInteractionComponent = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
	WidgetInteractionComponent->SetupAttachment(RightController);
	WidgetInteractionComponent->bShowDebug = true;
	WidgetInteractionComponent->InteractionDistance = 1000.0f;
	WidgetInteractionComponent->InteractionSource = EWidgetInteractionSource::World;
	WidgetInteractionComponent->PointerIndex = 0;
	WidgetInteractionComponent->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));

	// Laser pointer mesh (child of widget interaction)
	LaserPointerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserPointerMesh"));
	LaserPointerMesh->SetupAttachment(WidgetInteractionComponent);
	LaserPointerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaserPointerMesh->SetVisibility(false);
}

// Called when the game starts or when spawned
void AVrPianoPawn::BeginPlay()
{
	Super::BeginPlay();

	// Upewnij się, że kontrolery śledzą swoje źródła
	if (LeftController)
	{
		LeftController->SetTrackingMotionSource(FName("Left"));
	}
	if (RightController)
	{
		RightController->SetTrackingMotionSource(FName("Right"));
	}
}

// Called every frame
void AVrPianoPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateLaserPointer();
}

// Called to bind functionality to input
void AVrPianoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("TriggerLeft", IE_Pressed, this, &AVrPianoPawn::OnLeftTriggerPressed);
	PlayerInputComponent->BindAction("TriggerLeft", IE_Released, this, &AVrPianoPawn::OnLeftTriggerReleased);
	PlayerInputComponent->BindAction("TriggerRight", IE_Pressed, this, &AVrPianoPawn::OnRightTriggerPressed);
	PlayerInputComponent->BindAction("TriggerRight", IE_Released, this, &AVrPianoPawn::OnRightTriggerReleased);
}

void AVrPianoPawn::OnLeftTriggerPressed()
{
	// Możesz włączyć interakcję lewą ręką w przyszłości
	// WidgetInteractionComponent->PointerIndex = 1;
	// WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
}

void AVrPianoPawn::OnLeftTriggerReleased()
{
	// WidgetInteractionComponent->PointerIndex = 1;
	// WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
}

void AVrPianoPawn::OnRightTriggerPressed()
{
	WidgetInteractionComponent->PressPointerKey(EKeys::LeftMouseButton);
}

void AVrPianoPawn::OnRightTriggerReleased()
{
	WidgetInteractionComponent->ReleasePointerKey(EKeys::LeftMouseButton);
}

void AVrPianoPawn::UpdateLaserPointer()
{
	if (!WidgetInteractionComponent || !LaserPointerMesh) return;

	FVector Start = WidgetInteractionComponent->GetComponentLocation();
	FVector End = Start + WidgetInteractionComponent->GetForwardVector() * WidgetInteractionComponent->InteractionDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

	if (bHit)
	{
		LaserPointerMesh->SetVisibility(true);
		float Distance = FVector::Dist(Start, HitResult.Location);

		// Cylinder w UE ma oś Z jako główną => skalujemy w Z
		LaserPointerMesh->SetRelativeScale3D(FVector(0.01f, 0.01f, Distance / 100.0f));
		LaserPointerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, Distance / 2.0f));

		DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Red, false, 0, 0, 1.0f);
	}
	else
	{
		LaserPointerMesh->SetVisibility(false);
	}
}
