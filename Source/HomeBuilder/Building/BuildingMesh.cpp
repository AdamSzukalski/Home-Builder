// AS

#include "BuildingMesh.h"
#include "KismetProceduralMeshLibrary.h"
static void AddEdge(FMeshBuffers&B, FVector a,FVector b, float UStart, float UEnd, FVector CamLocal, float Thickness)
{
	FVector Direction = (b - a).GetSafeNormal();
	FVector Middle = (a + b) / 2.f;
	FVector ToCam = (CamLocal - Middle).GetSafeNormal();
	FVector Perpendicular = FVector::CrossProduct(Direction, ToCam).GetSafeNormal();
	if (Perpendicular.IsNearlyZero()) Perpendicular = FVector::CrossProduct(Direction, FVector::RightVector);
	Perpendicular.Normalize();
		
	int32 base = B.Vertices.Num();
	B.Vertices.Add(a - Perpendicular*Thickness); B.UVs.Add(FVector2D(UStart,0));
	B.Vertices.Add(a + Perpendicular*Thickness); B.UVs.Add(FVector2D(UStart,1));
	B.Vertices.Add(b - Perpendicular*Thickness); B.UVs.Add(FVector2D(UEnd,0));
	B.Vertices.Add(b + Perpendicular*Thickness); B.UVs.Add(FVector2D(UEnd,1));

	B.Triangles.Add(base+0); B.Triangles.Add(base+1); B.Triangles.Add(base+2);
	B.Triangles.Add(base+2);B.Triangles.Add(base+1);B.Triangles.Add(base+3);
}
static void AddLoop(FMeshBuffers&B, const TArray<FVector>& Points, bool bClosed, FVector CamLocal, float Thickness, float DashTile)
{
	//Makes Tile divide evenly for this loop
	float U = 0.f;
	if (bClosed)
	{
		float Total = 0.f;
		for (int32 e = 0 ; e < Points.Num() ; e++)
			Total += (Points[(e+1)%Points.Num()] - Points[e]).Size();
		float LocalTile = Total / FMath::Max(1, FMath::RoundToInt(Total/ DashTile));
			
		for (int32 e = 0; e < Points.Num(); e++)
		{
			FVector a = Points[e], b = Points[(e + 1) % Points.Num()];
			float Len = (b - a).Size();
			AddEdge(B, a, b, U, U + Len/LocalTile, CamLocal, Thickness);
			U += (b-a).Size() / LocalTile;
		}
	}
	else
	{
		float Total = 0.f;
		for (int32 e = 0 ; e < Points.Num() - 1 ; e++)
			Total += (Points[(e+1)] - Points[e]).Size();
		float LocalTile = Total / FMath::Max(1, FMath::RoundToInt(Total/ DashTile));
			
		for (int32 e = 0; e < Points.Num() - 1; e++)
		{
			FVector a = Points[e], b = Points[(e + 1)];
			float Len = (b - a).Size();
			AddEdge(B, a, b, U, U + Len/LocalTile, CamLocal, Thickness);
			U += (b-a).Size() / LocalTile;
		}
	}
}
namespace FBuildingMesh
{
	
    FMeshBuffers BuildWall(const FWallData& Wall, float WallStep)
    {
		FMeshBuffers B;
		const USplineComponent* SplineComponent = Wall.SplineComponent;
		if (!SplineComponent) return B;
		const float HalfThickness = Wall.Thickness / 2.f;
		const float Height = Wall.Height;
		const float Length = SplineComponent->GetSplineLength();
		
		TArray<float> Samples;
		for (float d = 0.f; d < Length; d += WallStep) Samples.Add(d);
		Samples.Add(Length);
		for (const FOpeningData& O : Wall.OpeningData)
		{
			Samples.Add(FMath::Clamp(O.Distance - O.Width*0.5f, 0.f, Length));
			Samples.Add(FMath::Clamp(O.Distance + O.Width*0.5f, 0.f, Length));
		}
		Samples.Sort();

		// vertex loop — one ring per sample
		for (int32 i = 0; i < Samples.Num(); i++)
		{
			float d = Samples[i];
			FVector Location = SplineComponent->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);
			FVector Right = SplineComponent->GetRightVectorAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);

			B.Vertices.Add(Location - Right*HalfThickness);//X1
			B.Vertices.Add(Location + Right*HalfThickness);//X2
			B.Vertices.Add(Location - Right*HalfThickness + FVector::UpVector*Height);//Y1
			B.Vertices.Add(Location + Right*HalfThickness + FVector::UpVector*Height);//Y2
		}
		
		auto AddQuad = [&](int32 a, int32 b, int32 c, int32 d)
		{
			B.Triangles.Add(a); B.Triangles.Add(b); B.Triangles.Add(c);
			B.Triangles.Add(c); B.Triangles.Add(b); B.Triangles.Add(d);
		};
		auto AddPanel = [&](FVector La, FVector Ra, FVector Lb, FVector Rb,
						float zLow, float zHigh, bool bTopCap, bool bBottomCap)
		{
			const FVector Up = FVector::UpVector;
			int32 b = B.Vertices.Num();
			B.Vertices.Add(La + Up*zLow);   // b+0 LA_low
			B.Vertices.Add(Ra + Up*zLow);   // b+1 RA_low
			B.Vertices.Add(La + Up*zHigh);  // b+2 LA_high
			B.Vertices.Add(Ra + Up*zHigh);  // b+3 RA_high
			B.Vertices.Add(Lb + Up*zLow);   // b+4 LB_low
			B.Vertices.Add(Rb + Up*zLow);   // b+5 RB_low
			B.Vertices.Add(Lb + Up*zHigh);  // b+6 LB_high
			B.Vertices.Add(Rb + Up*zHigh);  // b+7 RB_high

			AddQuad(b+0, b+2, b+4, b+6);              // left (outer) face
			AddQuad(b+1, b+5, b+3, b+7);              // right (inner) face
			if (bTopCap)    AddQuad(b+2, b+3,b+6, b+7); // top  (sill ledge / wall top)
			if (bBottomCap) AddQuad(b+0, b+1,b+4, b+5); // bottom (lintel underside)
		};
		for(int32 i = 0; i < Samples.Num() - 1; i++)
		{
			int32 r0 = i * 4;// first vert of ring i
			int32 r1 = (i + 1) * 4;  // first vert of ring i+1

			float dMid = (Samples[i] + Samples[i+1]) * 0.5f;   // irregular samples, so average them

			FVector La = B.Vertices[r0+0], Ra = B.Vertices[r0+1];  // ground corners (your precomputed bottom verts)
			FVector Lb = B.Vertices[r1+0], Rb = B.Vertices[r1+1];
			
			TArray<FVector2D>Gaps;
			FindOpeningsAt(Wall, dMid, Gaps);
			Gaps.Sort([](const FVector2D& A, const FVector2D& B){return A.X < B.X;});
			float z = 0.f;
			for (const FVector2D& G : Gaps)
			{
				if (G.X > z) AddPanel(La, Ra, Lb, Rb, z,  G.X,   false,  false);
				z = FMath::Max(z, G.Y);
			}
			if (z < Height) AddPanel(La, Ra, Lb, Rb, z, Height, true,  false);
		}
		const int32 LastRing = (Samples.Num() - 1) *4;
		AddQuad(0,1,2,3);
		AddQuad(LastRing, LastRing+2, LastRing+1, LastRing+3);
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
		return B;
    }
	FMeshBuffers BuildFloor(const USplineComponent* Spline, float WallStep, float FloorZOffset)
	{
		FMeshBuffers B;
	
		if (!Spline) return B;
		const int32 N = Spline->GetNumberOfSplinePoints();
		if (N < 3) return B;
	
		TArray<FVector> Outline;
		const float Length = Spline->GetSplineLength();
		const int32 NumRings = FMath::CeilToInt(Length / WallStep);
		for (int32 i = 0; i < NumRings; i++)
		{
			float d = i * WallStep;
			Outline.Add(Spline->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local));
		}
	
		float FloorZ = Outline[0].Z;
		for (const FVector& P : Outline) FloorZ = FMath::Max(FloorZ, P.Z);
		FloorZ += FloorZOffset;
		for (FVector& P : Outline) P.Z = FloorZ;
	
		FVector Center = FVector::ZeroVector;
		for (const FVector& P : Outline) Center += P;
		Center /= Outline.Num();
	
		for (const FVector& P : Outline) B.Vertices.Add(P);
		B.Vertices.Add(Center);

		const int32 M = Outline.Num();
		for (int32 i = 0; i < M; i++)
		{
			B.Triangles.Add(M);
			B.Triangles.Add((i + 1) % M);
			B.Triangles.Add(i);
		}

		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
		return B;
	}
	FMeshBuffers BuildRoof(const USplineComponent* Spline, int32 WallHeight, float RoofHeight)
	{
		FMeshBuffers B;
		if (!Spline) return B;
		const int32 N = Spline->GetNumberOfSplinePoints();
		if (N < 3) return B;
			
		TArray<FVector> P;
		for (int i = 0; i < N; i++)
		{
			P.Add(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
			P[i].Z += WallHeight;
		}
		FVector RidgeA, RidgeB;
		FVector Up = FVector::UpVector;
		if ((P[0] - P[1]).Size() > (P[1] - P[2]).Size())
		{
			RidgeA = FMath::Lerp(P[1], P[2], 0.5f) + Up * RoofHeight;
			RidgeB = FMath::Lerp(P[3], P[0], 0.5f) + Up * RoofHeight;

			//Roof Planes
			B.Vertices.Add(P[0]);
			B.Vertices.Add(P[1]);
			B.Vertices.Add(RidgeA);
			B.Vertices.Add(RidgeB);

			B.Vertices.Add(P[2]);
			B.Vertices.Add(P[3]);
			B.Vertices.Add(RidgeB);
			B.Vertices.Add(RidgeA);

			//Roof Ends
			B.Vertices.Add(P[1]);
			B.Vertices.Add(P[2]);
			B.Vertices.Add(RidgeA);
			B.Vertices.Add(P[3]);
			B.Vertices.Add(P[0]);
			B.Vertices.Add(RidgeB);
		}
		else
		{
			RidgeA = FMath::Lerp(P[0], P[1], 0.5f) + Up * RoofHeight;
			RidgeB = FMath::Lerp(P[2], P[3], 0.5f) + Up * RoofHeight;

			//Roof Planes
			B.Vertices.Add(P[1]);
			B.Vertices.Add(P[2]);
			B.Vertices.Add(RidgeA);
			B.Vertices.Add(RidgeB);

			B.Vertices.Add(P[3]);
			B.Vertices.Add(P[0]);
			B.Vertices.Add(RidgeB);
			B.Vertices.Add(RidgeA);

			//Roof Ends
			B.Vertices.Add(P[0]);
			B.Vertices.Add(P[1]);
			B.Vertices.Add(RidgeA);
			B.Vertices.Add(P[2]);
			B.Vertices.Add(P[3]);
			B.Vertices.Add(RidgeB);
		}
		// Roof planes
		for (int32 base : {0, 4})
		{
			B.Triangles.Add(base+0); B.Triangles.Add(base+2); B.Triangles.Add(base+1);
			B.Triangles.Add(base+0); B.Triangles.Add(base+3); B.Triangles.Add(base+2);
		}
		// Roof ends
		for (int32 base : {8, 11})
		{
			B.Triangles.Add(base+0); B.Triangles.Add(base+2); B.Triangles.Add(base+1);
		}
				
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
		return B;
	}
	FTransform TransformMesh(const UStaticMesh* Mesh, const USplineComponent* Spline,
		const FOpeningData& Opening, float Thickness)
	{
		FTransform Transform;

		Transform = Spline->GetTransformAtDistanceAlongSpline(Opening.Distance, ESplineCoordinateSpace::World);
		const FVector Up = FVector::UpVector;
		Transform.SetLocation(Transform.GetLocation() + Up*(Opening.SillHeight + Opening.OpeningHeight / 2));
		FRotator Rotator = Transform.Rotator();
		Rotator.Yaw += 90.f;
		Transform.SetRotation(Rotator.Quaternion());
	
		FVector Native = Mesh->GetBounds().BoxExtent * 2.f; 
		Transform.SetScale3D(FVector((Thickness + 2.f)/Native.X, Opening.Width/Native.Y, Opening.OpeningHeight/Native.Z));
		return Transform;
	}
	bool OpeningFits(const FWallData& Wall, const FOpeningData& O)
	{
		if (!Wall.SplineComponent) return false;
		const float Length = Wall.SplineComponent->GetSplineLength();
		const float HalfWidth = O.Width * 0.5f;
		return (O.SillHeight + O.OpeningHeight <= Wall.Height)
			&& (O.Distance - HalfWidth >= 0.f)
			&& (O.Distance + HalfWidth <= Length);
	}
	void FindOpeningsAt(const FWallData& Wall, float d, TArray<FVector2D>& OutGaps)
	{
    	for (const FOpeningData& O : Wall.OpeningData)
    	{
    		if (!OpeningFits(Wall, O)) continue;
    		float HalfWidth = O.Width / 2.f;
    		if (d >= O.Distance - HalfWidth && d <= O.Distance + HalfWidth)
    		{
    			OutGaps.Add(FVector2D(O.SillHeight, O.SillHeight + O.OpeningHeight));
    		}
    	}
	}
	FMeshBuffers BuildWallOutline(const FWallData& Wall, FVector CamLocal, float Thickness,
		float Height, float DashTile, float WallStep)
	{
    	TArray<FVector> Footprint;
    	TArray<FVector> Right;
    	TArray<FVector> BottomOut;
    	TArray<FVector> BottomIn;
    	TArray<FVector> TopOut;
    	TArray<FVector> TopIn;
    	
		FMeshBuffers B;
		USplineComponent* Spline = Wall.SplineComponent;
		float Length = Spline->GetSplineLength();
		int32 Num = FMath::CeilToInt(Length / WallStep);
		bool bClosed = Wall.bClosed;
    	float HalfThickness = Wall.Thickness / 2;
    	FVector Up = FVector::UpVector;
    	
		for (int32 i = 0; i < Num; i++)
		{
			Footprint.Add(Spline->GetLocationAtDistanceAlongSpline(FMath::Min(i * WallStep, Length), ESplineCoordinateSpace::Local));
			Right.Add(Spline->GetRightVectorAtDistanceAlongSpline(FMath::Min(i * WallStep, Length), ESplineCoordinateSpace::Local));

			BottomOut.Add(Footprint[i] + Right[i] * HalfThickness);
			BottomIn.Add(Footprint[i] - Right[i] * HalfThickness);
			TopOut.Add(BottomOut[i] + Up * Height);
			TopIn.Add(BottomIn[i] + Up * Height);
		}
		if (!bClosed)
		{
			Footprint.Add(Spline->GetLocationAtDistanceAlongSpline(Length, ESplineCoordinateSpace::Local));
			Right.Add(Spline->GetRightVectorAtDistanceAlongSpline(Length, ESplineCoordinateSpace::Local));

			BottomOut.Add(Footprint.Last() + Right.Last() * HalfThickness);
			BottomIn.Add(Footprint.Last() - Right.Last() * HalfThickness);
			TopOut.Add(BottomOut.Last() + Up * Height);
			TopIn.Add(BottomIn.Last() + Up * Height);
			
			AddEdge(B, BottomOut[0], BottomIn[0], 0,
				FMath::Max(1,FMath::RoundToInt((BottomIn[0]-BottomOut[0]).Size() / DashTile)), CamLocal, Thickness);
			AddEdge(B, BottomOut.Last(), BottomIn.Last(), 0,
			FMath::Max(1,FMath::RoundToInt((BottomIn.Last()-BottomOut.Last()).Size() / DashTile)), CamLocal, Thickness);
			AddEdge(B, TopOut[0], TopIn[0], 0,
			FMath::Max(1,FMath::RoundToInt((TopIn[0]-TopOut[0]).Size()/ DashTile)), CamLocal, Thickness);
			AddEdge(B, TopOut.Last(), TopIn.Last(), 0,
			FMath::Max(1,FMath::RoundToInt((TopIn.Last()-TopOut.Last()).Size()/ DashTile)), CamLocal, Thickness);
		}
		AddLoop(B, BottomOut, bClosed, CamLocal, Thickness, DashTile);
		AddLoop(B, BottomIn, bClosed, CamLocal, Thickness, DashTile);
		AddLoop(B, TopOut, bClosed, CamLocal, Thickness, DashTile);
		AddLoop(B, TopIn, bClosed, CamLocal, Thickness, DashTile);
		
		int32 NumPoints = Spline->GetNumberOfSplinePoints();
		for (int32 p = 0; p < NumPoints; p++)
		{
			float distance = Spline->GetDistanceAlongSplineAtSplinePoint(p);
			FVector Location = Spline->GetLocationAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local);
			FVector RightVector = Spline->GetRightVectorAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local);
			FVector BO = Location + RightVector * HalfThickness;
			FVector BI = Location - RightVector * HalfThickness;
			float UEnd = FMath::Max(1, FMath::RoundToInt((BO+Up*Height - BO).Size() / DashTile));
			AddEdge(B, BO, BO + Up*Height, 0,  UEnd, CamLocal, Thickness);
			AddEdge(B, BI, BI + Up*Height, 0, UEnd, CamLocal, Thickness);
		}
    	return B;
	}
	FMeshBuffers BuildOpeningOutline(const FOpeningData& Opening, const USplineComponent* Spline,
		FVector CamLocal, float Thickness, float OutlineThickness, float DashTile)
    {
    	FMeshBuffers B;
    	FVector Up = FVector::UpVector;
    	float HalfThickness = Thickness / 2;
    	
    	float d0 = Opening.Distance - Opening.Width / 2;
    	float d1 = Opening.Distance + Opening.Width / 2;

    	FVector L0 = Spline->GetLocationAtDistanceAlongSpline(d0, ESplineCoordinateSpace::Local);
    	FVector L1 = Spline->GetLocationAtDistanceAlongSpline(d1, ESplineCoordinateSpace::Local);

    	float SillHeight = Opening.SillHeight;
    	float HeadHeight = Opening.SillHeight + Opening.OpeningHeight;
    	FVector r0 = Spline->GetRightVectorAtDistanceAlongSpline(d0, ESplineCoordinateSpace::Local);
    	FVector r1 = Spline->GetRightVectorAtDistanceAlongSpline(d1, ESplineCoordinateSpace::Local);

    	FVector C0 = (L0 + Up*SillHeight) + r0 * HalfThickness;
    	FVector C1 = (L1 + Up*SillHeight) + r1 * HalfThickness;
    	FVector C2 = (L1 + Up*HeadHeight) + r1 * HalfThickness;
    	FVector C3 = (L0 + Up*HeadHeight) + r0 * HalfThickness;
    	FVector C4 = (L0 + Up*SillHeight) - r0 * HalfThickness;
    	FVector C5 = (L1 + Up*SillHeight) - r1 * HalfThickness;
    	FVector C6 = (L1 + Up*HeadHeight) - r1 * HalfThickness;
    	FVector C7 = (L0 + Up*HeadHeight) - r0 * HalfThickness;
		
    	TArray FrontCorners = {C0, C1, C2, C3};
    	TArray BackCorners = {C4, C5, C6, C7};
    	AddLoop(B, FrontCorners, true, CamLocal, OutlineThickness, DashTile);
    	AddLoop(B, BackCorners, true, CamLocal, OutlineThickness, DashTile);
    	for (int i = 0; i < 4; i++)
    	{
    		float UEnd = FMath::Max(1, FMath::RoundToInt((BackCorners[i]-FrontCorners[i]).Size() / DashTile));
    		AddEdge(B, FrontCorners[i], BackCorners[i], 0, UEnd, CamLocal, OutlineThickness);
    	}
    	return B;
    }
}

