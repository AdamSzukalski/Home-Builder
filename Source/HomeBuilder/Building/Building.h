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
USTRUCT()
struct FWallJunction
{
	GENERATED_BODY()
	int32 PointIndex = -1;    
	int32 TargetWall = -1;      
	float TargetDistance = 0.f; 
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
	UPROPERTY()
	TArray<FWallJunction> Junctions;
	float Height = 300.f;
	float Thickness = 100.f;
	bool bClosed = false;
	bool bRounded = false;
	float RoofRise = 180.f;
};
USTRUCT(BlueprintType)
struct FOpeningSnapshot
{
	GENERATED_BODY()
	float Distance;
	float Width = 120.f;
	float SillHeight = 0.f;
	float OpeningHeight = 200.f;
	bool bIsDoor = false;
};
USTRUCT(BlueprintType)
struct FWallSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FVector> Points;
	UPROPERTY()
	TArray<FOpeningSnapshot> OpeningSnapshot;
	UPROPERTY()
	TArray<FWallJunction> Junctions;
	float Height = 300.f;
	float Thickness = 100.f;
	bool bClosed = false;
	bool bRounded = false;
	float RoofRise = 180.f;
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
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_Context;
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_Undo;
	UPROPERTY(EditAnywhere, Category = "Building|Input")
	UInputAction* IA_Redo;

	UPROPERTY()
	TArray<FWallData> Walls;
	TArray<TArray<FWallSnapshot>> UndoStack;
	TArray<TArray<FWallSnapshot>> RedoStack;
	static constexpr float WallStep = 64.f;
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Wall")
	UMaterialInterface* WallMaterial;
	
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Floor")
	UMaterialInterface* FloorMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Tools|Roof")
	UMaterialInterface* RoofMaterial;
	UPROPERTY(EditAnywhere, Category = "Building|Tools|Roof")
	float RoofPitch = 0.6f;
	
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
	UFUNCTION()
	void PushUndoState();
	UFUNCTION()
	void Undo();
	UFUNCTION()
	void Redo();

	UFUNCTION(BlueprintPure, Category = "Building|Undo")
	bool CanUndo() const {return !UndoStack.IsEmpty();};
	UFUNCTION(BlueprintPure, Category = "Building|Redo")
	bool CanRedo() const {return !RedoStack.IsEmpty();};

	UFUNCTION(BlueprintPure, Category = "Building", meta = (WorldContext = "WorldContextObject"))
	static ABuilding* GetBuilding(const UObject* WorldContextObject);
	
	FVector MousePosition;
	bool UpdateMousePosition();
	//Building
	bool FindWallAtCursor(int32& OutIndex, float& OutKey);
	void BuildWallMesh(const FWallData& Wall, UProceduralMeshComponent* Target, bool bPreview);
	void RebuildWall(int32 Index);
	int32 TryMergeWalls(int32 MovedIndex);
	bool TryMakeTJunction(int32 MovedIndex);
	void ResolveAllJunctions(int32 SkipWall = -1);
	void FixJunctionsAfterRemove(int32 RemovedIndex);
	bool FormsTRoom(int32 Index) const;
	TArray<int32> GetConnectedWalls(int32 WallIndex);
	bool OpeningOverlaps(const FWallData& Wall, float Distance, float HalfWidth, float Sill, float Head,
	int32 IgnoreIndex = -1)const;
	void DeleteCorner(int32 WallIndex, int32 PointIndex);
	void FinalizeWallSnap(int32 Index);
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
	void RightClick();

	//Walls && Floor && Roof
	const float FloorZOffset = 2.f;
	const int32 UndoCap = 50;

	void ApplySplineType(USplineComponent* Spline, bool bRounded);
	int32 SpawnWall(const TArray<FVector>& WorldPoints, const FWallData& Template);
	TArray<FWallSnapshot> TakeSnapshot();
	void RestoreSnapshot(const TArray<FWallSnapshot>& Walls);
	void BuildFloorMesh(const USplineComponent* Spline, UProceduralMeshComponent* Target);
	void BuildRoofMesh(const USplineComponent* Spline, const int32 WallHeight, const int32 WallThickness,float RoofRise, UProceduralMeshComponent* Target);

	//Doors && Windows
	const float OpeningGap = 8.f;
	const float DoorHeight = 210.f;
	const float WindowHeight = 120.f;

	bool ComputeOpeningAtCursor(EBuildTool Tool, int32& OutWallIndex, FOpeningData& OutOpeningData, bool& bValid);
	void PlaceOpening(EBuildTool Tool);
};
