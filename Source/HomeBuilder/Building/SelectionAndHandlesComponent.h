// AS

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "GameTypes.h"
#include "SelectionAndHandlesComponent.generated.h"
UENUM(BlueprintType)
enum class EHandleType : uint8
{
	Corner,
	Segment,
	HeightKnob,
	OpeningMove,
	OpeningEdgeStart,
	OpeningEdgeEnd,
	OpeningHead,
	OpeningSill,
	RoofKnob
};
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

	UPROPERTY()
	TArray<UStaticMeshComponent*> Handles;
	UPROPERTY()
	TArray<UStaticMeshComponent*> HandleDecorations;
	UPROPERTY(EditAnywhere, Category = "Building|Handles")
	UMaterialInterface* HandleMaterial;
	UPROPERTY(EditAnywhere, Category = "Building|Handles|Meshes")
	UStaticMesh* CornerHandleMesh;
	UPROPERTY(EditAnywhere, Category = "Building|Handles|Meshes")
	UStaticMesh* MoveHandleMesh;
	UPROPERTY(EditAnywhere, Category = "Building|Handles")
	float HandlePickRadius = 25.f;
	UPROPERTY(EditAnywhere, Category = "Building|Handles")
	float HandleScale = .3;
	UPROPERTY(EditAnywhere, Category = "Building|Handles")
	float HandleScaleUpsizeOnHover = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Building|Handles")
	float SliderOffset = 80.f;

	UFUNCTION()
	void DeleteSelected();
	UFUNCTION()
	void DeleteHoveredCorner();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//Selection
	void SelectAtCursor();
	int32 PickHandle();
	//Handles
	bool TryBeginHandleDrag();
	bool TrySetCornerContext();
	void EndHandleDrag();
protected:
	ABuilding* Owner = nullptr;
	APlayerController* PlayerController = nullptr;
	
	FVector LastCameraLocation;
	FRotator LastCameraRotation;
	//Selection
	ESelectionType SelectionType = ESelectionType::None;
	int32 SelectedWallIndex = -1;
	int32 SelectedOpeningIndex = -1;
	TArray<int32> SelectedWalls;     
	UProceduralMeshComponent* SelectionOutline;
	int32 ContextWallIndex = -1;
	int32 ContextPointIndex = -1;
	
	void BuildSelectionOutline();

	//Handles
	bool bDragging = false;
	int32 DraggedPointIndex = -1;
	int32 HoveredHandleIndex = -1;
	FVector LastDragPoint;
	TArray<EHandleType> HandleTypes;
	TArray<int32> HandleWall;      
	TArray<int32> HandleLocalIndex; 

	UStaticMeshComponent* MakeHandle(UStaticMesh* Mesh, const FVector& Location, EHandleType Type, int32 WallIdx, int32 LocalIdx);
	UStaticMeshComponent* MakeHandleDecoration(UStaticMesh* Mesh);
	FVector HeightSliderBase(int32 WallIdx) const;
	FVector RoofSliderBase(int32 WallIdx) const;
	void RefreshHandles();
	bool GetCursorOnPlane(FVector PlanePoint,FVector PlaneNormal, FVector& Out);
	bool GetCursorOnAxis(FVector AxisPoint, FVector AxisDir, float& OutDist);
};
