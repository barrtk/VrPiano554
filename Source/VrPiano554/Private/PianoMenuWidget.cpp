#include "PianoMenuWidget.h"
#include "Components/Button.h"
#include "Logging/LogMacros.h"

void UPianoMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button)
	{
		Button->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButtonClicked);
	}

	if (Button_1)
	{
		Button_1->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton1Clicked);
	}

	if (Button_2)
	{
		Button_2->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton2Clicked);
	}

	if (Button_3)
	{
		Button_3->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton3Clicked);
	}

	if (Button_4)
	{
		Button_4->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton4Clicked);
	}

	if (Button_5)
	{
		Button_5->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton5Clicked);
	}

	if (Button_6)
	{
		Button_6->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton6Clicked);
	}

	if (Button_7)
	{
		Button_7->OnClicked.AddDynamic(this, &UPianoMenuWidget::OnButton7Clicked);
	}
}

void UPianoMenuWidget::OnButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Button clicked via C++"));
}

void UPianoMenuWidget::OnButton1Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Button_1 clicked via C++"));
}

void UPianoMenuWidget::OnButton2Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja X mniej - Button_2 clicked!"));
}

void UPianoMenuWidget::OnButton3Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja X więcej - Button_3 clicked!"));
}

void UPianoMenuWidget::OnButton4Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja Y mniej - Button_4 clicked!"));
}

void UPianoMenuWidget::OnButton5Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja Y więcej - Button_5 clicked!"));
}

void UPianoMenuWidget::OnButton6Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja Z mniej - Button_6 clicked!"));
}

void UPianoMenuWidget::OnButton7Clicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Kalibracja Z więcej - Button_7 clicked!"));
}