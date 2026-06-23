//AS
#include "GameHUD.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

AGameHUD::AGameHUD()
{
	
}

void AGameHUD::BeginPlay()
{
	Super::BeginPlay();

	// Spawn and add the HUD to the screen
	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
}

void AGameHUD::SetMode(EToolMode NewMode)
{
	CurrentMode = NewMode;
	OnModeChanged.Broadcast(CurrentMode);
}

void AGameHUD::SetTerrainTool(ETerrainTool NewTool)
{
	CurrentTerrainTool = NewTool;
}

void AGameHUD::SetBuildTool(EBuildTool NewTool)
{
	CurrentBuildTool = NewTool;
}

void AGameHUD::RequestDelete()
{
	OnDeleteRequested.Broadcast();
}

void AGameHUD::SetBrushSize(int32 NewSize)
{
	BrushSize = NewSize;
}

void AGameHUD::SetPaintTexture(EPaintTexture NewPaintTexture)
{
	CurrentPaintTexture = NewPaintTexture;
}

bool AGameHUD::SpendMoney(int32 Amount)
{
	if (Balance >= Amount)
	{
		Balance -= Amount;
		return true;
	}
	return false;
}
void AGameHUD::AddMoney(int32 Amount)
{
	Balance += Amount;
}

AGameHUD* AGameHUD::GetGameHUD(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PlayerController) return nullptr;
	return Cast<AGameHUD>(PlayerController->GetHUD());
}







