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
	UProceduralMeshComponent* WallMesh = nullptr;
	UPROPERTY()
	TArray<FOpeningData> OpeningData;
	float Height = 300.f;
	float Thickness = 100.f;
	bool bClosed = false;

};
class USelectionAndHandlesComponent;
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
	static constexpr float WallStep = 64.f;
	UPROPERTY(EditAnywhere, Category = "Building|Wall")
	UMaterialInterface* WallMaterial;
	// UPROPERTY(EditAnywhere, Category = "Building|Wall")
	// float WallHeight;
	
	UPROPERTY(EditAnywhere, Category = "Building|Floor")
	UMaterialInterface* FloorMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Roof")
	UMaterialInterface* RoofMaterial;
	UPROPERTY(EditAnywhere, Category = "Building|Roof")
	float RoofHeight;

	UPROPERTY(EditAnywhere, Category = "Building|Window")
	UStaticMesh* WindowMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Door")
	UStaticMesh* DoorMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	UMaterialInterface* PreviewMaterial;
	
	UPROPERTY(VisibleAnywhere, Category = "Building|Selection")
	USelectionAndHandlesComponent* Selection;
	
	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime) override;

	UFUNCTION()
	void HandleModeChange(EToolMode NewMode);

	//Building
	UFUNCTION()
	void BuildWallMesh(const FWallData& Wall, UProceduralMeshComponent* Target, bool bPreview);
	UFUNCTION()
	bool FindWallAtCursor(int32& OutIndex, float& OutKey);

protected:
	APlayerController* PlayerController = nullptr;
	AGameHUD* GameHUD = nullptr;
	FVector MousePosition;
	
	//Drawing
	UProceduralMeshComponent* MeshComponent = nullptr;
	USplineComponent* CurrentSpline = nullptr;
	bool bIsDrawingMode;
	const float CursorTolerance = 20.f;
	
	bool UpdateMousePosition(bool bSnap);
	void PlacePoint();
	void DrawingFinished();
	void DrawingCancelled();

	//Walls && Floor && Roof
	const float FloorZOffset = 2.f;
	
	void RebuildWall(int32 Index);
	void BuildFloorMesh(const USplineComponent* Spline, UProceduralMeshComponent* Target);
	void BuildRoofMesh(const USplineComponent* Spline, const int32 WallHeight, UProceduralMeshComponent* Target);

	//Doors && Windows
	UStaticMeshComponent* OpeningPreview = nullptr;
	const float OpeningGap = 8.f;
	const float DoorHeight = 210.f;
	const float WindowHeight = 120.f;

	bool ComputeOpeningAtCursor(EBuildTool Tool, int32& OutWallIndex, FOpeningData& OutOpeningData, bool& bValid);
	void PlaceOpening(EBuildTool Tool);
};
