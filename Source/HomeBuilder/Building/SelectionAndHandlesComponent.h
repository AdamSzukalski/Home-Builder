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
	OpeningSill
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
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//Selection
	void SelectAtCursor();
	//Handles
	bool TryBeginHandleDrag();
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
	UProceduralMeshComponent* SelectionOutline;
	
	void BuildSelectionOutline();

	//Handles
	bool bDragging = false;
	int32 DraggedPointIndex = -1;
	int32 HoveredHandleIndex = -1;
	FVector LastDragPoint;
	TArray<EHandleType> HandleTypes;

	UStaticMeshComponent* MakeHandle(UStaticMesh* Mesh, const FVector& Location, EHandleType Type);
	UStaticMeshComponent* MakeHandleDecoration(UStaticMesh* Mesh);
	FVector HeightSliderBase() const;
	void RefreshHandles();
	int32 PickHandle();
	bool GetCursorOnPlane(FVector PlanePoint,FVector PlaneNormal, FVector& Out);
	bool GetCursorOnAxis(FVector AxisPoint, FVector AxisDir, float& OutDist);
};
