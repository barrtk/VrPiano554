#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PianoMenuWidget.generated.h"

class UButton;

/**
 * 
 */
UCLASS()
class VRPIANO554_API UPianoMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_6;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* Button_7;

private:
	UFUNCTION()
	void OnButtonClicked();

	UFUNCTION()
	void OnButton1Clicked();

	UFUNCTION()
	void OnButton2Clicked();

	UFUNCTION()
	void OnButton3Clicked();

	UFUNCTION()
	void OnButton4Clicked();

	UFUNCTION()
	void OnButton5Clicked();

	UFUNCTION()
	void OnButton6Clicked();

	UFUNCTION()
	void OnButton7Clicked();
};
