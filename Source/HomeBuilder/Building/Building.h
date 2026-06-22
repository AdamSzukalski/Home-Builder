// AS

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "GameTypes.h"
#include "GameHUD.h"
#include "ProceduralMeshComponent.h"
#include "Building.generated.h"

USTRUCT(BlueprintType)
struct FOpeningData
{
	GENERATED_BODY()
	UPROPERTY()
	UStaticMeshComponent* OpeningMesh = nullptr;
	float Distance;
	float Width = 120.f;
	float SillHeight = 0.f; //0 - door, ~90 - window
	float OpeningHeight = 200.f;

	
};
USTRUCT(BlueprintType)
struct FWallData
{
	GENERATED_BODY()

	UPROPERTY()
	USplineComponent* SplineComponent = nullptr;
	UPROPERTY()
	TArray<FOpeningData> OpeningData;
	float Height = 300.f;
	float Thickness = 100.f;

};

UCLASS()
class HOMEBUILDER_API ABuilding : public AActor
{
	GENERATED_BODY()
	
public:	
	ABuilding();

	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputMappingContext* IMC_Building;
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_PlacePoint;
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_Commit;
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_Cancel;

	UPROPERTY(EditAnywhere, Category = "Building|Snapping")
	float SnapGridSize = 128.f;

	UPROPERTY()
	TArray<FWallData> Walls;
	UPROPERTY(EditAnywhere, Category = "Building|Wall")
	UMaterialInterface* WallMaterial;
	
	UPROPERTY(EditAnywhere, Category = "Building|Floor")
	UMaterialInterface* FloorMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Window")
	UStaticMesh* WindowMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Door")
	UStaticMesh* DoorMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	UMaterialInterface* PreviewMaterial;
	
	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime) override;

	UFUNCTION()
	void HandleModeChange(EToolMode NewMode);

protected:
	APlayerController* PlayerController;
	AGameHUD* GameHUD;
	FVector MousePosition;
	
	//Drawing
	UProceduralMeshComponent* MeshComponent;
	USplineComponent* CurrentSpline = nullptr;
	
	bool bIsDrawingMode;
	bool bDirty = false;
	
	bool UpdateMousePosition(bool bSnap);
	
	void PlacePoint();
	void DrawingFinished();
	void DrawingCancelled();

	//Walls && Floor
	static constexpr int32 WallPreviewSectionIndex = 1000;
	void BuildWallMesh(const FWallData& Wall, int32 SectionIndex);
	void BuildFloorMesh(const USplineComponent* SplineComponent, int32 SectionIndex);

	//Doors && Windows
	FTransform TransformMesh(const UStaticMesh* StaticMesh, const USplineComponent* SplineComponent,
		const FOpeningData& OpeningData, float Thickness);
	
	void PlaceOpening(EBuildTool Tool);
	bool FindOpeningAt(const FWallData& Wall, float d, float& OutSill, float& OutHead)const;


};
