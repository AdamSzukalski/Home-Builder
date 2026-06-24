// AS

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "GameTypes.h"
#include "SelectionAndHandlesComponent.generated.h"

class ABuilding;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HOMEBUILDER_API USelectionAndHandlesComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USelectionAndHandlesComponent();
	UPROPERTY(EditAnywhere, Category = "Building|Selection")
	UMaterialInterface* OutlineMaterial;
	UPROPERTY(EditAnywhere, Category = "Building|Selection")
	float OutlineThickness = 1.f;
	UPROPERTY(EditAnywhere, Category = "Building|Selection")
	float OutlineDashTile = 32.f;

	UFUNCTION()
	void DeleteSelected();

	void SelectAtCursor();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	ABuilding* Owner = nullptr;
	APlayerController* PlayerController = nullptr;

	ESelectionType SelectionType = ESelectionType::None;
	int32 SelectedWallIndex = -1;
	int32 SelectedOpeningIndex = -1;
	UProceduralMeshComponent* SelectionOutline;

	FVector LastCameraLocation;
	FRotator LastCameraRotation;
	
	void BuildSelectionOutline();
};
