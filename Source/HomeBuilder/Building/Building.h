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
	float SillHeight = 0.f;
	float OpeningHeight = 200.f;
	bool bIsDoor = false;
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

	UPROPERTY(VisibleAnywhere, Category = "Building|Tools|Wall")
	TArray<FWallData> Walls;
	static constexpr float WallStep = 64.f;
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Wall")
	UMaterialInterface* WallMaterial;
	
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Floor")
	UMaterialInterface* FloorMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Tools|Roof")
	UMaterialInterface* RoofMaterial;
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Roof")
	float RoofHeight;
	
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Window")
	UStaticMesh* WindowMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Door")
	UStaticMesh* DoorMesh;
	
	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	UMaterialInterface* PreviewMaterial;
	
	UPROPERTY(VisibleAnywhere)
	USelectionAndHandlesComponent* Selection;

	UPROPERTY(VisibleAnywhere, Category = "Building|Mesh")
	UProceduralMeshComponent* MeshComponent = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Building|Mesh")
	UStaticMeshComponent* OpeningPreview = nullptr;
	
	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime) override;

	UFUNCTION()
	void HandleModeChange(EToolMode NewMode);

	FVector MousePosition;
	bool UpdateMousePosition();
	//Building
	bool FindWallAtCursor(int32& OutIndex, float& OutKey);
	void BuildWallMesh(const FWallData& Wall, UProceduralMeshComponent* Target, bool bPreview);
	void RebuildWall(int32 Index);
protected:
	APlayerController* PlayerController = nullptr;
	AGameHUD* GameHUD = nullptr;
	
	//Drawing
	USplineComponent* CurrentSpline = nullptr;
	bool bIsDrawingMode = false;
	const float CursorTolerance = 20.f;
	const float WallCloseTolerance = 128.f;
	
	void PlacePoint();
	void DrawingFinished();
	void DrawingCancelled();
	void TickWallPreview();
	void TickOpeningPreview();

	//Walls && Floor && Roof
	const float FloorZOffset = 2.f;
	
	void BuildFloorMesh(const USplineComponent* Spline, UProceduralMeshComponent* Target);
	void BuildRoofMesh(const USplineComponent* Spline, const int32 WallHeight, UProceduralMeshComponent* Target);

	//Doors && Windows
	const float OpeningGap = 8.f;
	const float DoorHeight = 210.f;
	const float WindowHeight = 120.f;

	bool ComputeOpeningAtCursor(EBuildTool Tool, int32& OutWallIndex, FOpeningData& OutOpeningData, bool& bValid);
	bool OpeningOverlaps(const FWallData& Wall, float Distance, float HalfWidth)const;
	void PlaceOpening(EBuildTool Tool);
};
