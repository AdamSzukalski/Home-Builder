// AS

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "HudUI.generated.h"


UCLASS(BlueprintType, Blueprintable)
class HOMEBUILDER_API UHudUI : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="UI|Look")
	void UpdateButtons();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Look")
	FButtonStyle StyleActive;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Look")
	FButtonStyle StyleDefault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Look")
	FButtonStyle TextureStyleActive;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Look")
	FButtonStyle TextureStyleDefault;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Look")
	FColor ActiveTextColor = FColor::FromHex(TEXT("E9D8BEFF"));
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Look")
	FColor DefaultTextColor = FColor::FromHex(TEXT("41250BFF"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Look")
	TMap<FName, UButton*> Buttons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Functionality")
	USizeBox* PaintOptionsPanel;

protected:
	void ApplyButtonStyle(UButton* Button, bool bIsActive);
	void ApplyTextureButtonStyle(UButton* Button, bool bIsActive);
	
};



