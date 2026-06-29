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
	static FVector WallCornerOffset(const USplineComponent* Spline, float d, float HalfThickness, bool bClosed,
		bool bRounded)
	{
		const float Len = Spline->GetSplineLength();
		if (bClosed && d > Len - 1.f) d = 0.f;              
		const FVector R = Spline->GetRightVectorAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);
		if (bRounded) return R * HalfThickness;
		const int32 NumPts = Spline->GetNumberOfSplinePoints();
		const FVector P = Spline->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);
		for (int32 p = 0; p < NumPts; p++)
		{
			bool bInterior = bClosed || (p > 0 && p < NumPts - 1);
			if (!bInterior) continue;
			if (FMath::Abs(Spline->GetDistanceAlongSplineAtSplinePoint(p) - d) > 1.f) continue;
			int32 prev = (p == 0) ? NumPts - 1 : p - 1;
			int32 next = (p == NumPts - 1) ? 0 : p + 1;
			FVector ToPrev = (Spline->GetLocationAtSplinePoint(prev, ESplineCoordinateSpace::Local) - P).GetSafeNormal();
			FVector ToNext = (Spline->GetLocationAtSplinePoint(next, ESplineCoordinateSpace::Local) - P).GetSafeNormal();
			float SinHalf = FMath::Sqrt(FMath::Max(0.05f, (1.f - FVector::DotProduct(ToPrev, ToNext)) * 0.5f));
			FVector Bis = (-(ToPrev + ToNext)).GetSafeNormal();
			if (FVector::DotProduct(Bis, R) < 0.f) Bis = -Bis;
			return Bis * (HalfThickness / SinHalf);
		}
		return R * HalfThickness;
	}
    FMeshBuffers BuildWall(const FWallData& Wall, float WallStep)
    {
		FMeshBuffers B;
		const USplineComponent* Spline = Wall.SplineComponent;
		if (!Spline) return B;
		const float HalfThickness = Wall.Thickness / 2.f;
		const float Height = Wall.Height;
		const float Length = Spline->GetSplineLength();
		
		TArray<float> Samples;
		for (float d = 0.f; d < Length; d += WallStep) Samples.Add(d);
		Samples.Add(Length);
		for (const FOpeningData& O : Wall.OpeningData)
		{
			Samples.Add(FMath::Clamp(O.Distance - O.Width*0.5f, 0.f, Length));
			Samples.Add(FMath::Clamp(O.Distance + O.Width*0.5f, 0.f, Length));
		}
    	for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
    		Samples.Add(Spline->GetDistanceAlongSplineAtSplinePoint(i));
		Samples.Sort();
		
    	for (int32 i = 0; i < Samples.Num(); i++)
    	{
    		float d = Samples[i];
    		FVector P     = Spline->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);
    		FVector Offset = WallCornerOffset(Spline, d, HalfThickness, Wall.bClosed, Wall.bRounded);

    		B.Vertices.Add(P - Offset);                            //X1
    		B.Vertices.Add(P + Offset);                            //X2
    		B.Vertices.Add(P - Offset + FVector::UpVector*Height); //Y1
    		B.Vertices.Add(P + Offset + FVector::UpVector*Height); //Y2
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
			int32 r0 = i * 4;
			int32 r1 = (i + 1) * 4;  

			float dMid = (Samples[i] + Samples[i+1]) * 0.5f;  

			FVector La = B.Vertices[r0+0], Ra = B.Vertices[r0+1];  
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
		if (!Wall.bClosed)
		{
			const int32 LastRing = (Samples.Num() - 1) *4;
			AddQuad(0,1,2,3);
			AddQuad(LastRing, LastRing+2, LastRing+1, LastRing+3);
		}
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
		return B;
    }
	static bool PointInTri(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
    const float d1 = (P.X - B.X) * (A.Y - B.Y) - (A.X - B.X) * (P.Y - B.Y);
    const float d2 = (P.X - C.X) * (B.Y - C.Y) - (B.X - C.X) * (P.Y - C.Y);
    const float d3 = (P.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (P.Y - A.Y);
    return (d1 > 0 && d2 > 0 && d3 > 0) || (d1 < 0 && d2 < 0 && d3 < 0);
}
	static void TriangulateFloor(const TArray<FVector2D>& Poly, TArray<int32>& OutTris)
	{
	    const int32 N = Poly.Num();
	    if (N < 3) return;


	    float Area2 = 0.f;
	    for (int32 i = 0, j = N - 1; i < N; j = i++)
	        Area2 += Poly[j].X * Poly[i].Y - Poly[i].X * Poly[j].Y;

	    TArray<int32> Idx; Idx.Reserve(N);
	    if (Area2 > 0.f) for (int32 i = 0;     i < N;  i++) Idx.Add(i);
	    else             for (int32 i = N - 1; i >= 0; i--) Idx.Add(i);

	    int32 a = 0;
		int32 Guard = N * N;                               
	    while (Idx.Num() > 3 && Guard-- > 0)
	    {
	        const int32 n = Idx.Num();
	        const int32 ia = a % n, ib = (a + 1) % n, ic = (a + 2) % n;
	        const FVector2D& A = Poly[Idx[ia]];
	        const FVector2D& B = Poly[Idx[ib]];
	        const FVector2D& C = Poly[Idx[ic]];

	        const float Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	        bool bEar = Cross > KINDA_SMALL_NUMBER;         
	        for (int32 k = 0; k < n && bEar; k++)
	        {
	            if (k == ia || k == ib || k == ic) continue;
	            if (PointInTri(Poly[Idx[k]], A, B, C)) bEar = false;
	        }

	        if (bEar)
	        {
	            OutTris.Add(Idx[ia]); OutTris.Add(Idx[ic]); OutTris.Add(Idx[ib]);
	            Idx.RemoveAt(ib);
	            a = 0;
	        }
	        else a++;
	    }
	    if (Idx.Num() == 3)
	    {
	        OutTris.Add(Idx[0]); OutTris.Add(Idx[2]); OutTris.Add(Idx[1]);
	    }
	}
	FMeshBuffers BuildFloor(const USplineComponent* Spline, float WallStep, float FloorZOffset)
	{
		FMeshBuffers B;
	
		if (!Spline) return B;
		const int32 N = Spline->GetNumberOfSplinePoints();
		if (N < 3) return B;
	
    	TArray<FVector> Outline;
    	const float Length = Spline->GetSplineLength();

    	TArray<float> Dists;
    	for (float d = 0.f; d < Length; d += WallStep) Dists.Add(d);
    	for (int32 i = 0; i < N; i++)                      
    		Dists.Add(Spline->GetDistanceAlongSplineAtSplinePoint(i));
    	Dists.Sort();

		for (int32 i = Dists.Num() - 1; i > 0; i--)
			if (Dists[i] - Dists[i - 1] < 1.f) Dists.RemoveAt(i);
		
    	for (float d : Dists)
    		Outline.Add(Spline->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local));
	
		float FloorZ = Outline[0].Z;
		for (const FVector& P : Outline) FloorZ = FMath::Max(FloorZ, P.Z);
		FloorZ += FloorZOffset;
		for (FVector& P : Outline) P.Z = FloorZ;
		
		TArray<FVector> Poly;
		const int32 ON = Outline.Num();
		for (int32 i = 0; i < ON; i++)
		{
			const FVector A  = Outline[(i + ON - 1) % ON];
			const FVector C  = Outline[i];
			const FVector Nx = Outline[(i + 1) % ON];
			const FVector2D E0 = FVector2D(C.X - A.X,  C.Y - A.Y ).GetSafeNormal();
			const FVector2D E1 = FVector2D(Nx.X - C.X, Nx.Y - C.Y).GetSafeNormal();
			if (FMath::Abs(E0.X * E1.Y - E0.Y * E1.X) > 0.03f)  
				Poly.Add(C);
		}
		if (Poly.Num() < 3) Poly = Outline;                  

		for (const FVector& P : Poly) B.Vertices.Add(P);
		TArray<FVector2D> Poly2D;
		Poly2D.Reserve(Poly.Num());
		for (const FVector& P : Poly) Poly2D.Add(FVector2D(P.X, P.Y));
		TriangulateFloor(Poly2D, B.Triangles);

		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
		return B;
	}
	struct FFootprintFrame
	{
		TArray<FVector2D> Points; 
		FVector2D Origin;      
		FVector2D AxisX;         
		FVector2D AxisY;            
		float EaveZ = 0.f;          
	};
	static bool BuildFootprintFrame(const TArray<FVector>& Corners, FFootprintFrame& Out)
	{
		const int32 N = Corners.Num();
		if (N < 3) return false;
		
		int32 Best = -1;
		float BestLen = 0.f;
		for (int32 i = 0; i < N; i++)
		{
			const float Len = FVector::DistXY(Corners[(i + 1) % N], Corners[i]);
			if (Len > BestLen) { BestLen = Len; Best = i; }
		}
		if (Best == -1) return false;
		
		const FVector Edge = Corners[(Best + 1) % N] - Corners[Best];
		Out.AxisX = FVector2D(Edge.X, Edge.Y).GetSafeNormal();
		if (Out.AxisX.IsNearlyZero()) return false;     
		Out.AxisY  = FVector2D(-Out.AxisX.Y, Out.AxisX.X);
		Out.Origin = FVector2D(Corners[0].X, Corners[0].Y); 
		Out.EaveZ  = Corners[0].Z;
		
		Out.Points.Reset();
		Out.Points.Reserve(N);
		for (const FVector& C : Corners)
		{
			const FVector2D D = FVector2D(C.X, C.Y) - Out.Origin;
			Out.Points.Add(FVector2D(FVector2D::DotProduct(D, Out.AxisX),
									  FVector2D::DotProduct(D, Out.AxisY)));
		}
		return true;
	}
	static FVector FrameToLocal(const FFootprintFrame& F, const FVector2D& UV, float Z)
	{
		const FVector2D XY = F.Origin + UV.X * F.AxisX + UV.Y * F.AxisY;
		return FVector(XY.X, XY.Y, Z);
	}
	static bool AllCornersNear90(const TArray<FVector2D>& Pts, float ToleranceDeg = 5.f)
	{
		const int32 N = Pts.Num();
		if (N < 4) return false;                               
		const float Threshold = FMath::Sin(FMath::DegreesToRadians(ToleranceDeg));

		for (int32 i = 0; i < N; i++)
		{
			const FVector2D A = (Pts[(i + N - 1) % N] - Pts[i]).GetSafeNormal(); 
			const FVector2D B = (Pts[(i + 1) % N]     - Pts[i]).GetSafeNormal(); 
			if (A.IsNearlyZero() || B.IsNearlyZero()) return false;            
			
			if (FMath::Abs(FVector2D::DotProduct(A, B)) > Threshold) return false;
		}
		return true;
	}
	static void SnapToAxes(TArray<FVector2D>& Pts)
	{
		const int32 N = Pts.Num();
		for (int32 i = 0; i < N; i++)
		{
			FVector2D& A = Pts[i];
			FVector2D& B = Pts[(i + 1) % N];
			if (FMath::Abs(B.X - A.X) > FMath::Abs(B.Y - A.Y)) 
			{
				const float V = (A.Y + B.Y) * 0.5f;      
				A.Y = V; B.Y = V;
			}
			else                                           
			{
				const float U = (A.X + B.X) * 0.5f;          
				A.X = U; B.X = U;
			}
		}
	}
	struct FRoofRect { float U0, U1, V0, V1; };
	static bool PointInPolygon(const TArray<FVector2D>& Poly, const FVector2D& Q)
	{
	    bool bIn = false;
	    const int32 N = Poly.Num();
	    for (int32 i = 0, j = N - 1; i < N; j = i++)
	    {
	        const FVector2D& A = Poly[i];
	        const FVector2D& B = Poly[j];
	        if (((A.Y > Q.Y) != (B.Y > Q.Y)) &&
	            (Q.X < (B.X - A.X) * (Q.Y - A.Y) / (B.Y - A.Y) + A.X))
	            bIn = !bIn;
	    }
	    return bIn;
	}
	static void UniqueSorted(TArray<float>& Vals, float Tol = 1.0f)
	{
	    Vals.Sort();
	    for (int32 i = Vals.Num() - 1; i > 0; i--)
	        if (FMath::Abs(Vals[i] - Vals[i - 1]) <= Tol)
	            Vals.RemoveAt(i);
	}
	static TArray<FRoofRect> DecomposeRectilinear(const TArray<FVector2D>& Pts)
	{
	    TArray<FRoofRect> Out;
		
	    TArray<float> Us, Vs;
	    for (const FVector2D& P : Pts) { Us.Add(P.X); Vs.Add(P.Y); }
	    UniqueSorted(Us);
	    UniqueSorted(Vs);
	    if (Us.Num() < 2 || Vs.Num() < 2) return Out;

	    const int32 NU = Us.Num() - 1;   
	    const int32 NV = Vs.Num() - 1;   
		
	    TArray<bool> Inside; Inside.SetNumZeroed(NU * NV);
	    for (int32 iu = 0; iu < NU; iu++)
	        for (int32 iv = 0; iv < NV; iv++)
	        {
	            const FVector2D C((Us[iu] + Us[iu + 1]) * 0.5f,
	                              (Vs[iv] + Vs[iv + 1]) * 0.5f);
	            Inside[iu * NV + iv] = PointInPolygon(Pts, C);
	        }
		
	    TArray<bool> Used; Used.SetNumZeroed(NU * NV);
	    auto IsFree = [&](int32 iu, int32 iv) { return Inside[iu * NV + iv] && !Used[iu * NV + iv]; };

	    for (int32 iu = 0; iu < NU; iu++)
	        for (int32 iv = 0; iv < NV; iv++)
	        {
	            if (!IsFree(iu, iv)) continue;
        		
	            int32 eu = iu;
	            while (eu + 1 < NU && IsFree(eu + 1, iv)) eu++;
        		
	            int32 ev = iv;
	            for (bool bGrow = true; bGrow && ev + 1 < NV; )
	            {
	                for (int32 k = iu; k <= eu; k++)
	                    if (!IsFree(k, ev + 1)) { bGrow = false; break; }
	                if (bGrow) ev++;
	            }
        		
	            for (int32 a = iu; a <= eu; a++)
	                for (int32 b = iv; b <= ev; b++)
	                    Used[a * NV + b] = true;

	            Out.Add({ Us[iu], Us[eu + 1], Vs[iv], Vs[ev + 1] });
	        }
	    return Out;
	}
	static void AppendGable(FMeshBuffers& B, const FFootprintFrame& F, const FRoofRect& R, float RoofHeight)
	{
	    const float Eave  = F.EaveZ;
	    const float Ridge = F.EaveZ + RoofHeight;

		auto V = [&](float u, float v, float z) { return FrameToLocal(F, FVector2D(u, v), z); };
		auto AddQuad = [&](FVector a, FVector b, FVector c, FVector d)
		{
			const int32 base = B.Vertices.Num();
			B.Vertices.Add(a); B.Vertices.Add(b); B.Vertices.Add(c); B.Vertices.Add(d);
			B.Triangles.Add(base + 0); B.Triangles.Add(base + 2); B.Triangles.Add(base + 1);
			B.Triangles.Add(base + 0); B.Triangles.Add(base + 3); B.Triangles.Add(base + 2);
		};
		auto AddTri = [&](FVector a, FVector b, FVector c)
		{
			const int32 base = B.Vertices.Num();
			B.Vertices.Add(a); B.Vertices.Add(b); B.Vertices.Add(c);
			B.Triangles.Add(base + 0); B.Triangles.Add(base + 2); B.Triangles.Add(base + 1);
		};

	    if (R.U1 - R.U0 >= R.V1 - R.V0)        
	    {
	        const float vm = (R.V0 + R.V1) * 0.5f;
	        const FVector A  = V(R.U0, R.V0, Eave),  Bc = V(R.U1, R.V0, Eave);  // V0 eave
	        const FVector C  = V(R.U1, R.V1, Eave),  D  = V(R.U0, R.V1, Eave);  // V1 eave
	        const FVector rA = V(R.U0, vm, Ridge),   rB = V(R.U1, vm, Ridge);   // ridge ends

	        AddQuad(A, Bc, rB, rA);   // front slope (V0 side)
	        AddQuad(C, D, rA, rB);    // back slope  (V1 side)
	        AddTri(A, rA, D);         // gable end at U0
	        AddTri(Bc, C, rB);        // gable end at U1
	    }
	    else                               
	    {
	        const float um = (R.U0 + R.U1) * 0.5f;
	        const FVector A  = V(R.U0, R.V0, Eave),  D  = V(R.U0, R.V1, Eave);  // U0 eave
	        const FVector Bc = V(R.U1, R.V0, Eave),  C  = V(R.U1, R.V1, Eave);  // U1 eave
	        const FVector rA = V(um, R.V0, Ridge),   rB = V(um, R.V1, Ridge);   // ridge ends

	        AddQuad(D, A, rA, rB);    // left slope  (U0 side)
	        AddQuad(Bc, C, rB, rA);   // right slope (U1 side)
	        AddTri(A, Bc, rA);        // gable end at V0
	        AddTri(C, D, rB);         // gable end at V1
	    }
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

		FFootprintFrame F;
		if (BuildFootprintFrame(P, F) && AllCornersNear90(F.Points))
		{
			SnapToAxes(F.Points);
			for (const FRoofRect& R : DecomposeRectilinear(F.Points))
				AppendGable(B,F,R, RoofHeight);

			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
			return B;
		}
		
		if (N != 4) return B;
		
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
		bool bClosed = Wall.bClosed;
    	float HalfThickness = Wall.Thickness / 2;
    	FVector Up = FVector::UpVector;
    	
		const int32 NumPts = Spline->GetNumberOfSplinePoints();
		TArray<float> Dists;
		for (float d = 0.f; d < Length; d += WallStep) Dists.Add(d);
		for (int32 p = 0; p < NumPts; p++)
		{
			float pd = Spline->GetDistanceAlongSplineAtSplinePoint(p);
			if (pd > 0.5f && pd < Length - 0.5f) Dists.Add(pd);   // interior corners only
		}
		Dists.Sort();

		for (float d : Dists)
		{
			FVector P = Spline->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local);
			FVector Offset = WallCornerOffset(Spline, d, HalfThickness, bClosed, Wall.bRounded);
			Footprint.Add(P);
			Right.Add(Spline->GetRightVectorAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local));
			BottomOut.Add(P + Offset);
			BottomIn.Add(P - Offset);
			TopOut.Add(P + Offset + Up * Height);
			TopIn.Add(P - Offset + Up * Height);
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

