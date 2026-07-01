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
	static void SnapToGrid(TArray<FVector2D>& Pts, float Tol)
	{
		auto SnapAxis = [&](bool bX)
		{
			TArray<float> Sorted;
			for (const FVector2D& P : Pts) Sorted.Add(bX ? P.X : P.Y);
			Sorted.Sort();
			
			TArray<float> Reps;
			for (int32 i = 0; i < Sorted.Num(); )
			{
				int32 j = i + 1;
				while (j < Sorted.Num() && Sorted[j] - Sorted[j - 1] <= Tol) ++j;
				float sum = 0.f;
				for (int32 k = i; k < j; ++k) sum += Sorted[k];
				Reps.Add(sum / (j - i));
				i = j;
			}

			for (FVector2D& P : Pts)
			{
				auto& c = bX ? P.X : P.Y;
				float best = Reps[0];
				for (float r : Reps)
					if (FMath::Abs(r - c) < FMath::Abs(best - c)) best = r;
				c = best;
			}
		};
		SnapAxis(true);
		SnapAxis(false);
	}
	static void OffsetPolygonOutward(TArray<FVector2D>& Pts, float d)
	{
		const int32 N = Pts.Num();
		if (N < 3 || FMath::Abs(d) < KINDA_SMALL_NUMBER) return;

		double area2 = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& a = Pts[i];
			const FVector2D& b = Pts[(i + 1) % N];
			area2 += (double)a.X * b.Y - (double)b.X * a.Y;
		}
		const float sgn = (area2 >= 0.0) ? 1.f : -1.f;  

		TArray<FVector2D> ON; ON.SetNum(N);       
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D dir = (Pts[(i + 1) % N] - Pts[i]).GetSafeNormal();
			ON[i] = -sgn * FVector2D(-dir.Y, dir.X);
		}

		TArray<FVector2D> Out; Out.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D n1 = ON[(i + N - 1) % N];    
			const FVector2D n2 = ON[i];                
			const float den = 1.f + FVector2D::DotProduct(n1, n2);
			Out[i] = Pts[i] + ((FMath::Abs(den) < KINDA_SMALL_NUMBER)
								? FVector2D::ZeroVector
								: d * (n1 + n2) / den);
		}
		Pts = Out;
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
	static bool ChooseRidgeAlongU(const FFootprintFrame& F, const FRoofRect& R, bool bAllowFlip)
	{
		const bool bAspect = (R.U1 - R.U0) >= (R.V1 - R.V0);
		if (!bAllowFlip) return bAspect;       

		const float midU = 0.5f * (R.U0 + R.U1);
		const float midV = 0.5f * (R.V0 + R.V1);
		const float eps  = 1.0f;
		const bool nbrU = PointInPolygon(F.Points, FVector2D(R.U0 - eps, midV))
					   || PointInPolygon(F.Points, FVector2D(R.U1 + eps, midV));
		const bool nbrV = PointInPolygon(F.Points, FVector2D(midU, R.V0 - eps))
					   || PointInPolygon(F.Points, FVector2D(midU, R.V1 + eps));

		if (nbrU != nbrV) return nbrU;
		return bAspect; 
	}
	static void AppendGable(FMeshBuffers& B, const FFootprintFrame& F, const FRoofRect& R, float RoofPitch,float RidgeCapZ, bool bAllowFlip)
	{
	    const float Eave = F.EaveZ;

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
		const bool bUMajor = ChooseRidgeAlongU(F, R, bAllowFlip);
		
	    const float a0 = bUMajor ? R.U0 : R.V0;
	    const float a1 = bUMajor ? R.U1 : R.V1;
	    const float b0 = bUMajor ? R.V0 : R.U0;
	    const float b1 = bUMajor ? R.V1 : R.U1;
	    const float bm = 0.5f * (b0 + b1);
	    const float halfSpan = 0.5f * (b1 - b0);
		const float Ridge = FMath::Min(Eave + RoofPitch * halfSpan, RidgeCapZ);

	    auto P  = [&](float a, float b, float z) { return bUMajor ? V(a, b, z) : V(b, a, z); };
	    auto UV = [&](float a, float b)          { return bUMajor ? FVector2D(a, b) : FVector2D(b, a); };
	    auto Quad = [&](FVector q0, FVector q1, FVector q2, FVector q3)
	    { if (bUMajor) AddQuad(q0, q1, q2, q3); else AddQuad(q3, q2, q1, q0); };
	    auto Tri = [&](FVector t0, FVector t1, FVector t2)
	    { if (bUMajor) AddTri(t0, t1, t2); else AddTri(t2, t1, t0); };
			
	    const float eps = 1.0f;
	    const bool end0Interior = PointInPolygon(F.Points, UV(a0 - eps, bm));
	    const bool end1Interior = PointInPolygon(F.Points, UV(a1 + eps, bm));

		const float am = 0.5f * (a0 + a1);
		const bool b0Interior = PointInPolygon(F.Points, UV(am, b0 - eps));
		const bool b1Interior = PointInPolygon(F.Points, UV(am, b1 + eps));
			
		const float run = (RoofPitch > KINDA_SMALL_NUMBER) ? (Ridge - Eave) / RoofPitch : halfSpan;
		const float ra0 = a0 - (end0Interior ? run : 0.f);
		const float ra1 = a1 + (end1Interior ? run : 0.f);
			
	    Quad(P(a0, b0, Eave), P(a1, b0, Eave), P(ra1, bm, Ridge), P(ra0, bm, Ridge)); // b0 slope
	    Quad(P(a1, b1, Eave), P(a0, b1, Eave), P(ra0, bm, Ridge), P(ra1, bm, Ridge)); // b1 slope
			
	    // if (!end0Interior) Tri(P(a0, b0, Eave), P(a0, bm, Ridge), P(a0, b1, Eave));
	    // if (!end1Interior) Tri(P(a1, b0, Eave), P(a1, b1, Eave), P(a1, bm, Ridge));

		if (!end0Interior)
		{
			if (!b0Interior) Tri(P(a0, b0, Eave), P(a0, bm, Ridge), P(a0, bm, Eave)); // b0 half
			if (!b1Interior) Tri(P(a0, bm, Eave), P(a0, bm, Ridge), P(a0, b1, Eave)); // b1 half
		}
		if (!end1Interior)
		{
			if (!b0Interior) Tri(P(a1, b0, Eave), P(a1, bm, Eave), P(a1, bm, Ridge)); // b0 half
			if (!b1Interior) Tri(P(a1, bm, Eave), P(a1, b1, Eave), P(a1, bm, Ridge)); // b1 half
		}
	}
	struct FWaveVtx
	{
	    FVector2D P0;       
	    FVector2D Vel;       
	    float     tBirth = 0.f;
	    int32     Prev = -1, Next = -1;
	    int32     LeftEdge = -1, RightEdge = -1;
	    int32     Node = -1; 
	    bool      bAlive = true;
	};
	static bool BuildHipRoof(const FFootprintFrame& F, float RoofPitch, FMeshBuffers& B)
	{
	    const int32 N = F.Points.Num();
	    if (N < 3) return false;

	  
	    double area2 = 0.0;
	    for (int32 i = 0; i < N; ++i)
	    {
	        const FVector2D& a = F.Points[i];
	        const FVector2D& b = F.Points[(i + 1) % N];
	        area2 += (double)a.X * b.Y - (double)b.X * a.Y;
	    }
	    TArray<FVector2D> Poly = F.Points;
	    if (area2 < 0.0) { for (int32 i = 0; i < N / 2; ++i) Poly.Swap(i, N - 1 - i); }

	  
	    TArray<FVector2D> EN; EN.SetNum(N);
	    for (int32 i = 0; i < N; ++i)
	    {
	        const FVector2D d = (Poly[(i + 1) % N] - Poly[i]).GetSafeNormal();
	        EN[i] = FVector2D(-d.Y, d.X);
	    }

	   
	    for (int32 i = 0; i < N; ++i)
	    {
	        const FVector2D d0 = Poly[i] - Poly[(i + N - 1) % N];
	        const FVector2D d1 = Poly[(i + 1) % N] - Poly[i];
	        if (d0.X * d1.Y - d0.Y * d1.X <= 0.f) return false;
	    }

	    const float Eave = F.EaveZ;
	    auto AddNode = [&](const FVector2D& uv, float t) -> int32
	    { return B.Vertices.Add(FrameToLocal(F, uv, Eave + RoofPitch * t)); };
	    auto MakeVel = [&](int32 le, int32 re) -> FVector2D
	    {
	        const FVector2D n1 = EN[le], n2 = EN[re];
	        const float den = 1.f + FVector2D::DotProduct(n1, n2);
	        return (FMath::Abs(den) < KINDA_SMALL_NUMBER) ? FVector2D::ZeroVector : (n1 + n2) / den;
	    };

	 
	    TArray<FWaveVtx> W;
		W.SetNum(N);
		W.Reserve(2 * N);
	    for (int32 i = 0; i < N; ++i)
	    {
	        FWaveVtx& v = W[i];
	        v.P0 = Poly[i];
	        v.LeftEdge = (i + N - 1) % N; v.RightEdge = i;
	        v.Prev = (i + N - 1) % N;     v.Next = (i + 1) % N;
	        v.Vel = MakeVel(v.LeftEdge, v.RightEdge);
	        v.Node = AddNode(Poly[i], 0.f);
	    }

	  
	    auto EdgeTime = [&](int32 is, float& outT, FVector2D& outQ) -> bool
	    {
	        const FWaveVtx& Vs = W[is];
	        const FWaveVtx& Ve = W[Vs.Next];
	        const FVector2D a = Vs.P0 - Ve.P0;
	        const FVector2D b = Vs.Vel - Ve.Vel;
	        const float bb = FVector2D::DotProduct(b, b);
	        if (bb < KINDA_SMALL_NUMBER) return false;          
	        const float t = -FVector2D::DotProduct(a, b) / bb;
	        if (t <= FMath::Max(Vs.tBirth, Ve.tBirth) + 1e-3f) return false;
	        if ((a + b * t).Size() > 1.0f) return false;          
	        outT = t; outQ = Vs.P0 + Vs.Vel * t;
	        return true;
	    };

	    struct FEv { float T; int32 Vs; int32 NextSnap; FVector2D Q; };
	    auto Pred = [](const FEv& A, const FEv& C){ return A.T < C.T; };
	    TArray<FEv> Q;
	    for (int32 i = 0; i < N; ++i)
	    {
	        float t; FVector2D q;
	        if (EdgeTime(i, t, q)) Q.HeapPush({ t, i, W[i].Next, q }, Pred);
	    }
		
		TArray<TArray<int32>> startChain; startChain.SetNum(N);
		TArray<TArray<int32>> endChain;   endChain.SetNum(N);
		TArray<int32> apex; apex.Init(-1, N);
		for (int32 e = 0; e < N; ++e)
		{
			startChain[e].Add(W[e].Node);         
			endChain[e].Add(W[(e + 1) % N].Node);   
		}
		
	    int32 alive = N;
	    while (alive > 1 && Q.Num() > 0)
	    {
	        FEv e; Q.HeapPop(e, Pred);
	        FWaveVtx& Vs = W[e.Vs];
	        if (!Vs.bAlive || Vs.Next != e.NextSnap) continue;  
	        FWaveVtx& Ve = W[Vs.Next];
	        if (!Ve.bAlive) continue;

	        const int32 q = AddNode(e.Q, e.T);
	    	const int32 m  = Vs.RightEdge;  
	    	const int32 eL = Vs.LeftEdge;   
	    	const int32 eR = Ve.RightEdge;  
	    	apex[m] = q;
	    	endChain[eL].Add(q);
	    	startChain[eR].Add(q);
    		
	        const int32 iU = W.Add(FWaveVtx());
	        FWaveVtx& U = W[iU];
	        U.P0 = e.Q;                 
	        U.tBirth = e.T;
	        U.LeftEdge = Vs.LeftEdge; U.RightEdge = Ve.RightEdge;
	        U.Prev = Vs.Prev;         U.Next = Ve.Next;
	        U.Node = q;
	        U.Vel = MakeVel(U.LeftEdge, U.RightEdge);
	        U.P0 = e.Q - U.Vel * e.T;   
	        W[U.Prev].Next = iU;
	        W[U.Next].Prev = iU;
	        Vs.bAlive = Ve.bAlive = false;
	        --alive;

	        float t; FVector2D qq;
	        if (EdgeTime(U.Prev, t, qq)) Q.HeapPush({ t, U.Prev, W[U.Prev].Next, qq }, Pred);
	        if (EdgeTime(iU,     t, qq)) Q.HeapPush({ t, iU,     U.Next,        qq }, Pred);
	    }

		auto FanNormalUp = [&](const TArray<int32>& L) -> bool
		{
			const FVector A = B.Vertices[L[0]];
			FVector n(0);
			for (int32 i = 1; i + 1 < L.Num(); ++i)
				n += FVector::CrossProduct(B.Vertices[L[i]] - A, B.Vertices[L[i + 1]] - A);
			return n.Z >= 0.f;
		};

		for (int32 e = 0; e < N; ++e)
		{
			TArray<int32> loop;
			for (int32 i = 0; i < endChain[e].Num(); ++i) loop.Add(endChain[e][i]);
			if (apex[e] >= 0) loop.Add(apex[e]);
			for (int32 i = startChain[e].Num() - 1; i >= 0; --i) loop.Add(startChain[e][i]);
			
			TArray<int32> L;
			for (int32 idx : loop)
				if (L.Num() == 0 || L.Last() != idx) L.Add(idx);
			if (L.Num() >= 2 && L[0] == L.Last()) L.Pop();
			if (L.Num() < 3) continue;

			const bool up = FanNormalUp(L);
			for (int32 i = 1; i + 1 < L.Num(); ++i)
			{
				if (up) { B.Triangles.Add(L[0]); B.Triangles.Add(L[i + 1]); B.Triangles.Add(L[i]);     }
				else    { B.Triangles.Add(L[0]); B.Triangles.Add(L[i]);     B.Triangles.Add(L[i + 1]); }
				
			}
		}
		return true;
	}
	void SnapSplineTo90(USplineComponent* Spline)
	{
		if (!Spline) return;
		const int32 N = Spline->GetNumberOfSplinePoints();
		if (N < 4) return;

		TArray<FVector> P;
		for (int32 i = 0; i < N; ++i)
			P.Add(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));

		FFootprintFrame F;
		if (!BuildFootprintFrame(P, F)) return;
		if (!AllCornersNear90(F.Points)) return;  

		SnapToAxes(F.Points);
		SnapToGrid(F.Points, 10.f);

		for (int32 i = 0; i < N; ++i)
			Spline->SetLocationAtSplinePoint(i, FrameToLocal(F, F.Points[i], P[i].Z),
											 ESplineCoordinateSpace::Local, false);
		Spline->UpdateSpline();
	}
	FMeshBuffers BuildRoof(const USplineComponent* Spline, int32 WallHeight, float RoofRise, float EaveOffset)
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
		const bool bFrame = BuildFootprintFrame(P, F);
		if (bFrame) OffsetPolygonOutward(F.Points, EaveOffset);
		if (bFrame && AllCornersNear90(F.Points))
		{
			SnapToAxes(F.Points);
			SnapToGrid(F.Points, 10.f);
			const TArray<FRoofRect> Rects = DecomposeRectilinear(F.Points);
			if (Rects.Num() == 0) return B;

			int32 PrimaryIdx = 0;
			float bestArea = -1.f;
			for (int32 i = 0; i < Rects.Num(); ++i)
			{
				const float area = (Rects[i].U1 - Rects[i].U0) * (Rects[i].V1 - Rects[i].V0);
				if (area > bestArea) { bestArea = area; PrimaryIdx = i; }
			}
			const FRoofRect& Pr = Rects[PrimaryIdx];
			const float PrimaryHalfSpan = 0.5f * FMath::Min(Pr.U1 - Pr.U0, Pr.V1 - Pr.V0);
			const float RoofPitch = (PrimaryHalfSpan > KINDA_SMALL_NUMBER) ? RoofRise / PrimaryHalfSpan : 0.f;
			const float RidgeCapZ = F.EaveZ + RoofRise;

			for (int32 i = 0; i < Rects.Num(); ++i)
				AppendGable(B, F, Rects[i], RoofPitch, RidgeCapZ, /*bAllowFlip=*/ i != PrimaryIdx);

			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
			return B;
		}
		FVector2D Min( BIG_NUMBER,  BIG_NUMBER);
		FVector2D Max(-BIG_NUMBER, -BIG_NUMBER);
		for (const FVector2D& p : F.Points)
		{
			Min.X = FMath::Min(Min.X, p.X); Min.Y = FMath::Min(Min.Y, p.Y);
			Max.X = FMath::Max(Max.X, p.X); Max.Y = FMath::Max(Max.Y, p.Y);
		}
		const float HalfSpan = 0.5f * FMath::Min(Max.X - Min.X, Max.Y - Min.Y);
		const float RoofPitch = (HalfSpan > KINDA_SMALL_NUMBER) ? RoofRise / HalfSpan : 0.f;
		if (bFrame && BuildHipRoof(F, RoofPitch, B))
		{
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(B.Vertices, B.Triangles, B.UVs, B.Normals, B.Tangents);
			return B;
		}
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
	FMeshBuffers BuildRoofOutline(const USplineComponent* Spline, int32 WallHeight, float RoofRise,
        float EaveOffset, FVector CamLocal, float OutlineThickness, float DashTile)
    {
        FMeshBuffers B;
        FMeshBuffers R = BuildRoof(Spline, WallHeight, RoofRise, EaveOffset); 
        if (R.Triangles.Num() == 0) return B;
    
        auto Key  = [](const FVector& p){ return FIntVector(FMath::RoundToInt(p.X), FMath::RoundToInt(p.Y), FMath::RoundToInt(p.Z)); };
        auto Less = [](const FIntVector& a, const FIntVector& b){ return a.X!=b.X ? a.X<b.X : (a.Y!=b.Y ? a.Y<b.Y : a.Z<b.Z); };
    
        struct FRec { FVector A, B; FVector N; int32 Count = 0; bool bFeature = false; };
        TMap<FString, FRec> Edges;
    
        auto AddTriEdge = [&](const FVector& a, const FVector& b, const FVector& n)
        {
            FIntVector ka = Key(a), kb = Key(b);
            if (Less(kb, ka)) Swap(ka, kb);
            FString id = FString::Printf(TEXT("%d_%d_%d_%d_%d_%d"), ka.X,ka.Y,ka.Z, kb.X,kb.Y,kb.Z);
            FRec& r = Edges.FindOrAdd(id);
            if (r.Count == 0) { r.A = a; r.B = b; r.N = n; }
            else if (FVector::DotProduct(r.N, n) < 0.99f) r.bFeature = true;   
            r.Count++;
        };
    
        for (int32 t = 0; t + 2 < R.Triangles.Num(); t += 3)
        {
            const FVector& v0 = R.Vertices[R.Triangles[t]];
            const FVector& v1 = R.Vertices[R.Triangles[t+1]];
            const FVector& v2 = R.Vertices[R.Triangles[t+2]];
            FVector n = FVector::CrossProduct(v1 - v0, v2 - v0).GetSafeNormal();
            AddTriEdge(v0, v1, n);
            AddTriEdge(v1, v2, n);
            AddTriEdge(v2, v0, n);
        }
    
        for (const auto& Pair : Edges)
        {
            const FRec& r = Pair.Value;
            if (!(r.Count == 1 || r.bFeature)) continue;   
            float Len = (r.B - r.A).Size();
            AddEdge(B, r.A, r.B, 0.f, Len / DashTile, CamLocal, OutlineThickness);
        }
        return B;
    }
}

