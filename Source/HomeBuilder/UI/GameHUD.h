//AS
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameTypes.h"
#include "GameHUD.generated.h"

UCLASS(Blueprintable)
class HOMEBUILDER_API AGameHUD : public AHUD
{
	GENERATED_BODY()
public:
	AGameHUD();

protected:
	virtual void BeginPlay() override;
public:
	// Mode & Tool State
	UPROPERTY(BlueprintReadWrite, Category="UI|State")
	EToolMode CurrentMode = EToolMode::Terrain;

	UPROPERTY(BlueprintReadWrite, Category="UI|State")
	ETerrainTool CurrentTerrainTool = ETerrainTool::Raise;

	UPROPERTY(BlueprintReadWrite, Category="UI|State")
	EBuildTool CurrentBuildTool = EBuildTool::Floor;

	// Paint Texture
	UPROPERTY(BlueprintReadWrite, Category="UI|State")
	EPaintTexture CurrentPaintTexture = EPaintTexture::Grass;

	// Brush Size (Terrain)
	UPROPERTY(BlueprintReadWrite, Category="UI|State")
	int32 BrushSize = 5;

	// Money
	UPROPERTY(BlueprintReadWrite, Category="UI|Economy")
	int32 Balance = 4250;

	// Widget class to spawn (assign in BP child)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|Widgets")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	// The actual widget instance
	UPROPERTY(BlueprintReadOnly, Category="UI|Widgets")
	TObjectPtr<UUserWidget> HUDWidget;

	// Called from Blueprint buttons
	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetMode(EToolMode NewMode);

	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FOnModeChanged OnModeChanged;

	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetTerrainTool(ETerrainTool NewTool);

	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetBuildTool(EBuildTool NewTool);

	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetBrushSize(int32 NewSize);

	UFUNCTION(BlueprintCallable, Category = "UI|State")
	void SetPaintTexture(EPaintTexture NewPaintTexture);

	// Economy
	UFUNCTION(BlueprintCallable, Category = "UI|Economy")
	bool SpendMoney(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "UI|Economy")
	void AddMoney(int32 Amount);

	// Helper to get HUD from anywhere
	UFUNCTION(BlueprintPure, Category = "UI", meta = (WorldContext="WorldContextObject"))
	static AGameHUD* GetGameHUD(const UObject* WorldContextObject);
};
