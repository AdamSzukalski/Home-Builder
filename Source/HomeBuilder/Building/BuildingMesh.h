// AS

#pragma once

#include "CoreMinimal.h"
#include "Building.h"
#include "ProceduralMeshComponent.h"

struct  FMeshBuffers
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
};

namespace FBuildingMesh
{
	FMeshBuffers BuildWall(const FWallData& Wall, float WallStep);
	FMeshBuffers BuildFloor(const USplineComponent* Spline, float WallStep, float FloorZOffset);
	FMeshBuffers BuildRoof(const USplineComponent* Spline, int32 WallHeight, float RoofHeight);
	FTransform TransformMesh(const UStaticMesh* Mesh, const USplineComponent* Spline,
		const FOpeningData& Opening, float Thickness);
	void FindOpeningsAt(const FWallData& Wall, float d, TArray<FVector2D>& OutGaps);
	bool OpeningFits(const FWallData& Wall, const FOpeningData& O);
	FMeshBuffers BuildWallOutline(const FWallData& Wall, FVector CamLocal, float Thickness,
		float Height, float DashTile, float WallStep);
	FMeshBuffers BuildOpeningOutline(const FOpeningData& Opening, const USplineComponent* Spline,
		FVector CamLocal, float Thickness,float OutlineThickness, float DashTile);
}

