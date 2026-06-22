// AS


#include "HudUI.h"

#include "GameHUD.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
void UHudUI::UpdateButtons()
{
	AGameHUD* GameHUD = Cast<AGameHUD>(GetWorld()->GetFirstPlayerController()->GetHUD());
	if (!GameHUD) return;
	
	ApplyButtonStyle(Buttons.FindRef("Build"), GameHUD->CurrentMode == EToolMode::Build);
	ApplyButtonStyle(Buttons.FindRef("Terrain"), GameHUD->CurrentMode == EToolMode::Terrain);
	ApplyButtonStyle(Buttons.FindRef("Shop"), GameHUD->CurrentMode == EToolMode::Shop);

	ApplyButtonStyle(Buttons.FindRef("Raise"), GameHUD->CurrentTerrainTool == ETerrainTool::Raise);
	ApplyButtonStyle(Buttons.FindRef("Lower"), GameHUD->CurrentTerrainTool == ETerrainTool::Lower);
	ApplyButtonStyle(Buttons.FindRef("Paint"), GameHUD->CurrentTerrainTool == ETerrainTool::Paint);
	ApplyButtonStyle(Buttons.FindRef("Erase"), GameHUD->CurrentTerrainTool == ETerrainTool::Erase);
	
	ApplyButtonStyle(Buttons.FindRef("Wall"), GameHUD->CurrentBuildTool == EBuildTool::Wall);
	ApplyButtonStyle(Buttons.FindRef("Door"), GameHUD->CurrentBuildTool == EBuildTool::Door);
	ApplyButtonStyle(Buttons.FindRef("Window"), GameHUD->CurrentBuildTool == EBuildTool::Window);
	ApplyButtonStyle(Buttons.FindRef("Roof"), GameHUD->CurrentBuildTool == EBuildTool::Roof);
	ApplyButtonStyle(Buttons.FindRef("Delete"), GameHUD->CurrentBuildTool == EBuildTool::Delete);

	if (!PaintOptionsPanel) return;
	PaintOptionsPanel->SetVisibility(GameHUD->CurrentTerrainTool == ETerrainTool::Paint? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	ApplyTextureButtonStyle(Buttons.FindRef("Grass"), GameHUD->CurrentPaintTexture == EPaintTexture::Grass);
	ApplyTextureButtonStyle(Buttons.FindRef("Dirt"), GameHUD->CurrentPaintTexture == EPaintTexture::Dirt);
	ApplyTextureButtonStyle(Buttons.FindRef("Pavement"), GameHUD->CurrentPaintTexture == EPaintTexture::Pavement);
	ApplyTextureButtonStyle(Buttons.FindRef("Rocks"), GameHUD->CurrentPaintTexture == EPaintTexture::Rocks);
	ApplyTextureButtonStyle(Buttons.FindRef("Sand"), GameHUD->CurrentPaintTexture == EPaintTexture::Sand);
}

void UHudUI::ApplyButtonStyle(UButton* Button, bool bIsActive)
{
	if (!Button) return;
	
	Button->SetStyle(bIsActive ? StyleActive : StyleDefault);
	
	if (UTextBlock* SimpleText = Cast<UTextBlock>(Button->GetChildAt(0)))
	{
		SimpleText->SetColorAndOpacity(bIsActive ? ActiveTextColor : DefaultTextColor);
		return;
	}
	
	UVerticalBox* VBox = Cast<UVerticalBox>(Button->GetChildAt(0));
	if (!VBox) return;

	UOverlay* Overlay = Cast<UOverlay>(VBox->GetChildAt(0));
	UTextBlock* Text = Cast<UTextBlock>(VBox->GetChildAt(1));
	if (!Overlay || !Text) return;

	UImage* ImageWhite = Cast<UImage>(Overlay->GetChildAt(0));
	UImage* ImageBrown = Cast<UImage>(Overlay->GetChildAt(1));
	if (!ImageWhite || !ImageBrown) return;
	
	Text->SetColorAndOpacity(bIsActive ? ActiveTextColor : DefaultTextColor);
	
	ImageWhite->SetVisibility(bIsActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ImageBrown->SetVisibility(bIsActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UHudUI::ApplyTextureButtonStyle(UButton* Button, bool bIsActive)
{
	if (!Button) return;
	Button->SetStyle(bIsActive ? TextureStyleActive : TextureStyleDefault);
}

