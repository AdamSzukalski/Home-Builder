// AS

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "GameHUD.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TerrainMesh.generated.h"

UCLASS()
class HOMEBUILDER_API ATerrainMesh : public AActor
{
	GENERATED_BODY()
	
public:	
	ATerrainMesh();
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Mesh")
	int32 GridSize = 64;
	UPROPERTY(EditAnywhere, Category = "Terrain|Mesh")
	int32 QuadSize = 64;
	UPROPERTY(EditAnywhere, Category = "Terrain|Mesh")
	UMaterialInterface* Material;
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Input")
	UInputMappingContext* IMC_Terraform;
	UPROPERTY(EditAnywhere, Category = "Terrain|Input")
	UInputAction* IA_Sculpt;
	UPROPERTY(EditAnywhere, Category = "Terrain|Input")
	UInputAction* IA_Paint;

	UPROPERTY(EditAnywhere, Category = "Terrain|Sculpt")
	float SculptStrength = 150;
	UPROPERTY(EditAnywhere, Category = "Terrain|Sculpt")
	float SmoothingStrength = 0.05f;
	
	UPROPERTY(EditAnywhere, Category = "Terrain|Paint")
	float PaintStrength = 150;

	UPROPERTY(EditAnywhere, Category = "Terrain|Brush")
	UMaterialInterface* BrushMaterial;
	UPROPERTY(EditAnywhere, Category = "Terrain|Brush")
	float ProjectionDepth = 500.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void HandleModeChange(EToolMode NewMode);
protected:
	//Main Variables and Functions
	APlayerController* PlayerController;
	AGameHUD* GameHUD;
	FVector BrushCenter;
	FVector BrushNormal;
	
	bool UpdateBrushPosition();
	void CalculateBrushBounds(int32 TexelX, int32 TexelY,
		int32& MinX, int32& MinY, int32& MaxX, int32& MaxY);
	void WorldToTexel(const FVector& WorldPos, int32& VertexX, int32& VertexY) const;

	//Terrain Generation - Assigning Variables and Functions
	UProceduralMeshComponent* MeshComponent;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	
	void GenerateTerrain();
	void RecalculateNormalsInRegion(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY);
	void UpdateMesh();

	//Terraformation Tools - Assigning Variables and Functions
	UDecalComponent* BrushDecal;
	
	//Sculpting
	bool bIsSculptingMode;
	TMap<FIntPoint, float> HeightRemainder;
	
	void SculptStarted();
	void SculptFinished();

	void ApplyHeightChange(int32 TexelX, int32 TexelY, bool bIsRaising, float DeltaTime);
	//Painting
	bool bIsPaintingMode;
	TMap<FIntPoint, float> WeightRemainder;
	
	void PaintStarted();
	void PaintFinished();

	void ApplyWeightChange(int32 TexelX, int32 TexelY, bool bIsPainting, float DeltaTime);
};
