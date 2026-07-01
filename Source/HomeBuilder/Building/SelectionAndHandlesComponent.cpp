// AS


#include "SelectionAndHandlesComponent.h"
#include "Building.h"
#include "BuildingMesh.h"

USelectionAndHandlesComponent::USelectionAndHandlesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}
void USelectionAndHandlesComponent::BeginPlay()
{
	Super::BeginPlay();
	
	Owner = Cast<ABuilding>(GetOwner());
	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!Owner) return;
	SelectionOutline = NewObject<UProceduralMeshComponent>(GetOwner());
	SelectionOutline->SetupAttachment(Owner->GetRootComponent());
	SelectionOutline->RegisterComponent();

	SelectionOutline->SetVisibility(false);
	SelectionOutline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void USelectionAndHandlesComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!PlayerController) return;
	//Making the Outline face camera every tick if something is selected
	if (bDragging)
	{
		if (!HandleTypes.IsValidIndex(DraggedPointIndex))return;
		USplineComponent* Spline = Owner->Walls[SelectedWallIndex].SplineComponent;
		int32 NumPoints = Spline->GetNumberOfSplinePoints();
		float PlaneZ = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Z;
		FOpeningData Saved;
		if (SelectionType == ESelectionType::Opening)
			Saved = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
		switch (HandleTypes[DraggedPointIndex])
		{
			case EHandleType::Corner:
			{
					FVector P;
					if (!GetCursorOnPlane(FVector(0,0,PlaneZ), FVector::UpVector, P)) return;
					P.Z = PlaneZ;
					int32 LocalPt = HandleLocalIndex[DraggedPointIndex];
					FWallJunction* J = nullptr;
					for (FWallJunction& Jn : Owner->Walls[SelectedWallIndex].Junctions)
						if (Jn.PointIndex == LocalPt) { J = &Jn; break; }
					if (J && Owner->Walls.IsValidIndex(J->TargetWall) && Owner->Walls[J->TargetWall].SplineComponent)
					{
						// junction corner: slide along the wall it's attached to so it stays connected
						USplineComponent* T = Owner->Walls[J->TargetWall].SplineComponent;
						float Key = T->FindInputKeyClosestToWorldLocation(P);
						J->TargetDistance = T->GetDistanceAlongSplineAtSplineInputKey(Key);
						FVector C = T->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
						Spline->SetLocationAtSplinePoint(LocalPt, C, ESplineCoordinateSpace::World, true);
					}
					else
						Spline->SetLocationAtSplinePoint(LocalPt, P, ESplineCoordinateSpace::World, true);
					Owner->RebuildWall(SelectedWallIndex);
					break;
			}
			case EHandleType::Segment:
			{
					FVector P;
					if (!GetCursorOnPlane(FVector(0,0,PlaneZ), FVector::UpVector, P)) return;
					int32 Seg = HandleLocalIndex[DraggedPointIndex];
					int32 A = Seg;int32 B = (Seg + 1)	% NumPoints;
					FVector Delta = P - LastDragPoint;
					for (int32 Idx : {A, B})
					{
						FVector L = Spline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::World);
						Spline->SetLocationAtSplinePoint(Idx, L + FVector(Delta.X, Delta.Y, 0),
						ESplineCoordinateSpace::World, true);
					}
					LastDragPoint = P;
					Owner->RebuildWall(SelectedWallIndex);
					break;
			}
			case EHandleType::HeightKnob:
			{
					FVector Base = HeightSliderBase(SelectedWallIndex);
					float HeightAlong;
					if (!GetCursorOnAxis(Base, FVector::UpVector, HeightAlong)) break;
					float NewHeight = FMath::Clamp(HeightAlong, 100.f, 600.f);
					for (int32 wi : SelectedWalls)
						if (Owner->Walls.IsValidIndex(wi)) { Owner->Walls[wi].Height = NewHeight; Owner->RebuildWall(wi); }
					break;
			}
			case EHandleType::OpeningMove:
			{
				FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
				if (O.bIsDoor)
				{
					FVector P;
					if (!GetCursorOnPlane(FVector(0, 0, PlaneZ + O.SillHeight + O.OpeningHeight / 2.f), FVector::UpVector, P)) break;
					float Delta = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(P))
						- Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(LastDragPoint));
					LastDragPoint = P;
					float Half = O.Width / 2.f;
					O.Distance = FMath::Clamp(O.Distance + Delta, Half, Spline->GetSplineLength() - Half);
				}
				else
				{
					float dc = O.Distance;
					float WallHeight = Owner->Walls[SelectedWallIndex].Height;
					FVector Base = Spline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World);
					FVector Normal = Spline->GetRightVectorAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World);
					FVector P;
					if (!GetCursorOnPlane(Base, Normal, P)) break;
					float Delta = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(P))
						- Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(LastDragPoint));
					LastDragPoint = P;
					float Half = O.Width / 2.f;
					O.Distance = FMath::Clamp(O.Distance + Delta, Half, Spline->GetSplineLength() - Half);
					O.SillHeight = FMath::Clamp((P.Z - Base.Z) - O.OpeningHeight / 2.f, 0.f, WallHeight - O.OpeningHeight);
				}
				break;
			}
			case EHandleType::OpeningEdgeStart:
			case EHandleType::OpeningEdgeEnd:
			{
				FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
				FVector P;
				if (!GetCursorOnPlane(FVector(0, 0, PlaneZ + O.SillHeight + O.OpeningHeight / 2.f), FVector::UpVector, P)) break;
				float Delta = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(P))
					- Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(LastDragPoint));
				LastDragPoint = P;
				float d0 = O.Distance - O.Width / 2.f;
				float d1 = O.Distance + O.Width / 2.f;
				const float MinWidth = 40.f;
				if (HandleTypes[DraggedPointIndex] == EHandleType::OpeningEdgeStart)
					d0 = FMath::Clamp(d0 + Delta, 0.f, d1 - MinWidth);
				else
					d1 = FMath::Clamp(d1 + Delta, d0 + MinWidth, Spline->GetSplineLength());
				O.Width = d1 - d0;
				O.Distance = (d0 + d1) / 2.f;
				break;
			}
			case EHandleType::OpeningHead:
			{
				FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
				FVector Base = Spline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World);
				float OutDist;
				if (!GetCursorOnAxis(Base, FVector::UpVector, OutDist)) break;
				float WallHeight = Owner->Walls[SelectedWallIndex].Height;
				const float MinHeight = 40.f;
				float Head = FMath::Clamp(OutDist, O.SillHeight + MinHeight, WallHeight);
				O.OpeningHeight = Head - O.SillHeight;
				break;
			}
			case EHandleType::OpeningSill:
			{
				FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
				FVector Base = Spline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World);
				float OutDist;
				if (!GetCursorOnAxis(Base, FVector::UpVector, OutDist)) break;
				const float MinHeight = 40.f;
				float Head = O.SillHeight + O.OpeningHeight;
				float NewSill = FMath::Clamp(OutDist, 0.f, Head - MinHeight);
				O.SillHeight = NewSill;
				O.OpeningHeight = Head - NewSill;
				break;
			}
			case EHandleType::RoofKnob:
			{
					FVector Base = RoofSliderBase(SelectedWallIndex);
					float Along;
					if (!GetCursorOnAxis(Base, FVector::UpVector, Along)) break;
					float NewRise = FMath::Clamp(Along, 0.5f, 400.f);
					if(NewRise < 5.f) NewRise = 0.5f;
					for (int32 wi : SelectedWalls)
						if (Owner->Walls.IsValidIndex(wi))
						{ Owner->Walls[wi].RoofRise = NewRise; Owner->RebuildWall(wi); }
					break;
			}
		}
		if (SelectionType == ESelectionType::Opening)
		{
			FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
			if (Owner->OpeningOverlaps(Owner->Walls[SelectedWallIndex], O.Distance, O.Width / 2.f,
				O.SillHeight, O.SillHeight + O.OpeningHeight, SelectedOpeningIndex))
				O = Saved;
			Owner->RebuildWall(SelectedWallIndex);
		}
		if (SelectionType == ESelectionType::Wall)
			Owner->ResolveAllJunctions();
		RefreshHandles();
		if (Handles.IsValidIndex(DraggedPointIndex))
			Handles[DraggedPointIndex]->SetWorldScale3D(FVector(HandleScale * HandleScaleUpsizeOnHover));
		BuildSelectionOutline();
		return;
	}
	if (SelectionType != ESelectionType::None)
	{
		int32 NewHover = PickHandle();
		if (NewHover != HoveredHandleIndex)
		{
			if (Handles.IsValidIndex(HoveredHandleIndex)) Handles[HoveredHandleIndex]->SetWorldScale3D(FVector(HandleScale));
			if (Handles.IsValidIndex(NewHover)) Handles[NewHover]->SetWorldScale3D(FVector(HandleScale * HandleScaleUpsizeOnHover));
			HoveredHandleIndex = NewHover;
		}
		FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
		if (CameraLocation.Equals(LastCameraLocation, 0.5) && CameraRotation.Equals(LastCameraRotation, 0.1)) return;
		LastCameraLocation = CameraLocation;
		LastCameraRotation = CameraRotation;
		BuildSelectionOutline();
	}
}
//Selection
void USelectionAndHandlesComponent::DeleteSelected()
{
	if (SelectionType == ESelectionType::None) return;
	if (SelectionType == ESelectionType::Wall)
	{
		TArray<int32> ToDelete = SelectedWalls;
		ToDelete.Sort();
		for (int32 n = ToDelete.Num() - 1; n >= 0; n--)
		{
			int32 idx = ToDelete[n];
			if (!Owner->Walls.IsValidIndex(idx)) continue;
			for (const auto& OpeningData : Owner->Walls[idx].OpeningData)
				if (OpeningData.OpeningMesh) OpeningData.OpeningMesh->DestroyComponent();
			if (Owner->Walls[idx].WallMesh) Owner->Walls[idx].WallMesh->DestroyComponent();
			if (Owner->Walls[idx].SplineComponent) Owner->Walls[idx].SplineComponent->DestroyComponent();
			Owner->Walls.RemoveAt(idx);
			Owner->FixJunctionsAfterRemove(idx);
		}
	}
	else if(SelectionType == ESelectionType::Opening)
	{
		Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex].OpeningMesh->DestroyComponent();
		Owner->Walls[SelectedWallIndex].OpeningData.RemoveAt(SelectedOpeningIndex);
		Owner->BuildWallMesh(Owner->Walls[SelectedWallIndex], Owner->Walls[SelectedWallIndex].WallMesh, false);
	}
	SelectionType = ESelectionType::None;
	SelectedWallIndex = -1;
	SelectedOpeningIndex = -1;
	BuildSelectionOutline();
	RefreshHandles();
}
void USelectionAndHandlesComponent::DeleteHoveredCorner()
{
	if (!Owner->Walls.IsValidIndex(ContextWallIndex)) return;
	Owner->DeleteCorner(ContextWallIndex, ContextPointIndex);
	if (Owner->Walls.IsValidIndex(SelectedWallIndex))
		SelectedWalls = Owner->GetConnectedWalls(SelectedWallIndex);
	else {SelectionType = ESelectionType::None; SelectedWalls.Empty();}
	RefreshHandles();
	BuildSelectionOutline();
	ContextWallIndex = ContextPointIndex = -1;
}
void USelectionAndHandlesComponent::SelectAtCursor()
{
	FHitResult Hit;
	if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		for (int32 i = 0; i < Owner->Walls.Num(); i++)
		{
			if (Owner->Walls[i].WallMesh != Hit.GetComponent()) continue;
			float EaveZ = Owner->Walls[i].SplineComponent
							->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Z
						  + Owner->Walls[i].Height;
			if (Hit.ImpactPoint.Z > EaveZ + 1.f) 
			{
				SelectionType    = ESelectionType::Roof;
				SelectedWallIndex = i;
				SelectedWalls    = Owner->GetConnectedWalls(i);
				BuildSelectionOutline();
				RefreshHandles();
				return;
			}
		}
	}
	int32 WallIndex;
	float SplineKey;
	if (!Owner->FindWallAtCursor(WallIndex, SplineKey))
	{
		SelectionType = ESelectionType::None;
		BuildSelectionOutline(); //clears + hides
		RefreshHandles();
		return;
	}
	USplineComponent* Spline = Owner->Walls[WallIndex].SplineComponent;
	float Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(SplineKey);
	bool bFound = false;
	for (int i = 0; i < Owner->Walls[WallIndex].OpeningData.Num(); i++)
	{
		float BaseZ = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).Z;
		float ClickHeight = Owner->MousePosition.Z - BaseZ;
		FOpeningData OpeningData = Owner->Walls[WallIndex].OpeningData[i];
		if (Distance >= OpeningData.Distance - OpeningData.Width / 2 &&
		Distance <= OpeningData.Distance + OpeningData.Width / 2 &&
		ClickHeight >= OpeningData.SillHeight &&
		ClickHeight <= OpeningData.SillHeight + OpeningData.OpeningHeight)
		{
			SelectionType = ESelectionType::Opening;
			SelectedWallIndex = WallIndex;
			SelectedOpeningIndex = i;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		SelectionType = ESelectionType::Wall;
		SelectedWallIndex = WallIndex;
		SelectedWalls = Owner->GetConnectedWalls(WallIndex);
	}
	BuildSelectionOutline();
	RefreshHandles();
}
void USelectionAndHandlesComponent::BuildSelectionOutline()
{
	SelectionOutline->ClearMeshSection(0);
	if (SelectionType == ESelectionType::None)
	{
		SelectionOutline->SetVisibility(false);
		return;
	}
	FVector CamWorld = PlayerController->PlayerCameraManager->GetCameraLocation();
	FVector CamLocal = Owner->GetActorTransform().InverseTransformPosition(CamWorld);
	
	USplineComponent* Spline = Owner->Walls[SelectedWallIndex].SplineComponent;
	float Thickness = Owner->Walls[SelectedWallIndex].Thickness;
	if (SelectionType == ESelectionType::Wall)
	{
		FMeshBuffers All;
		for (int32 wi : SelectedWalls)
		{
			if (!Owner->Walls.IsValidIndex(wi)) continue;
			FMeshBuffers B = FBuildingMesh::BuildWallOutline(Owner->Walls[wi], CamLocal, OutlineThickness,
				Owner->Walls[wi].Height, OutlineDashTile, ABuilding::WallStep);
			int32 base = All.Vertices.Num();
			All.Vertices.Append(B.Vertices);
			All.Normals.Append(B.Normals);
			All.UVs.Append(B.UVs);
			All.VertexColors.Append(B.VertexColors);
			All.Tangents.Append(B.Tangents);
			for (int32 t : B.Triangles) All.Triangles.Add(t + base);
		}
		SelectionOutline->CreateMeshSection(0, All.Vertices, All.Triangles, All.Normals, All.UVs, All.VertexColors, All.Tangents, false);
	}
	else if (SelectionType == ESelectionType::Opening)
	{
		FMeshBuffers B = FBuildingMesh::BuildOpeningOutline(Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex],
			Spline, CamLocal, Thickness,OutlineThickness, OutlineDashTile);
		SelectionOutline->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, false);
	}
	else if (SelectionType == ESelectionType::Roof)
	{
		int32 rep = SelectedWallIndex;
		FMeshBuffers B = FBuildingMesh::BuildRoofOutline(
		Owner->Walls[rep].SplineComponent,
		Owner->Walls[rep].Height,
		Owner->Walls[rep].RoofRise,      
		Owner->Walls[rep].Thickness * 0.5f,
		CamLocal, OutlineThickness, OutlineDashTile);
		SelectionOutline->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, false);
	}


	SelectionOutline->SetVisibility(true);
	if (OutlineMaterial) SelectionOutline->SetMaterial(0, OutlineMaterial);
}
//Handles
UStaticMeshComponent* USelectionAndHandlesComponent::MakeHandle(UStaticMesh* Mesh, const FVector& Location, EHandleType Type, int32 WallIdx, int32 LocalIdx)
{
	UStaticMeshComponent* H = NewObject<UStaticMeshComponent>(Owner);
	H->SetStaticMesh(Mesh);
	if (HandleMaterial)H->SetMaterial(0, HandleMaterial);
	H->SetupAttachment(Owner->GetRootComponent());
	H->RegisterComponent();
	H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H->SetWorldScale3D(FVector(HandleScale));
	H->SetWorldLocation(Location);
	Handles.Add(H);
	HandleTypes.Add(Type);
	HandleWall.Add(WallIdx);
	HandleLocalIndex.Add(LocalIdx);
	return H;
}
UStaticMeshComponent* USelectionAndHandlesComponent::MakeHandleDecoration(UStaticMesh* Mesh)
{
	UStaticMeshComponent* D = NewObject<UStaticMeshComponent>(Owner);
	D->SetStaticMesh(Mesh);
	if (HandleMaterial)D->SetMaterial(0, HandleMaterial);
	D->SetupAttachment(Owner->GetRootComponent());
	D->RegisterComponent();
	D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandleDecorations.Add(D);
	return D;
}
FVector USelectionAndHandlesComponent::HeightSliderBase(int32 WallIdx) const
{
    USplineComponent* Spline = Owner->Walls[WallIdx].SplineComponent;
    int32 NumPoints = Spline->GetNumberOfSplinePoints();
    bool  bClosed   = Owner->Walls[WallIdx].bClosed;
    float HalfThick = Owner->Walls[WallIdx].Thickness * 0.5f;

    int32 Corner = 0;
    FVector P = Spline->GetLocationAtSplinePoint(Corner, ESplineCoordinateSpace::World);

    bool bHasPrev = bClosed || Corner > 0;
    bool bHasNext = bClosed || Corner < NumPoints - 1;
    int32 PrevIdx = (Corner - 1 + NumPoints) % NumPoints;
    int32 NextIdx = (Corner + 1) % NumPoints;

    FVector MeshCorner;  
    FVector GapDir;       

    if (bHasPrev && bHasNext)
    {
        FVector ToPrev = (Spline->GetLocationAtSplinePoint(PrevIdx, ESplineCoordinateSpace::World) - P).GetSafeNormal2D();
        FVector ToNext = (Spline->GetLocationAtSplinePoint(NextIdx, ESplineCoordinateSpace::World) - P).GetSafeNormal2D();
        FVector Outward = (-(ToPrev + ToNext)).GetSafeNormal2D();
        float SinHalf = FMath::Sqrt(FMath::Max(0.f, (1.f - FVector::DotProduct(ToPrev, ToNext)) * 0.5f));

        if (Outward.IsNearlyZero() || SinHalf < KINDA_SMALL_NUMBER)
        {
            Outward = FVector(-ToNext.Y, ToNext.X, 0.f);
            MeshCorner = P + Outward * HalfThick;
        }
        else MeshCorner = P + Outward * (HalfThick / SinHalf);
        GapDir = Outward;                       
    }
    else
    {
        int32 Other = bHasNext ? NextIdx : PrevIdx;
        FVector Dir  = (Spline->GetLocationAtSplinePoint(Other, ESplineCoordinateSpace::World) - P).GetSafeNormal2D();
        FVector Perp = FVector(-Dir.Y, Dir.X, 0.f);
        MeshCorner = P + Perp * HalfThick;           
        GapDir     = (Perp - Dir).GetSafeNormal2D();
    }
    return MeshCorner + GapDir * SliderOffset;
}
FVector USelectionAndHandlesComponent::RoofSliderBase(int32 WallIdx) const
{
	USplineComponent* Spline = Owner->Walls[WallIdx].SplineComponent;
	int32 N = Spline->GetNumberOfSplinePoints();
	FVector Sum = FVector::ZeroVector;
	for (int32 i = 0; i < N; i++)
		Sum += Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
	FVector C = Sum / N;                             
	C.Z = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Z
		+ Owner->Walls[WallIdx].Height;                 
	return C;
}
void USelectionAndHandlesComponent::RefreshHandles()
{
	for (UStaticMeshComponent* H : Handles) H->DestroyComponent();
	Handles.Empty();
	HandleTypes.Empty();
	HandleWall.Empty();
	HandleLocalIndex.Empty();
	HoveredHandleIndex = -1;
	for (UStaticMeshComponent* D : HandleDecorations) D->DestroyComponent();
	HandleDecorations.Empty();

	if (SelectionType == ESelectionType::None) return;

	if (SelectionType == ESelectionType::Roof)
	{
		int32 rep = SelectedWallIndex;
		if (!Owner->Walls.IsValidIndex(rep)) return;
		FVector Base = RoofSliderBase(rep);
		float Rise = Owner->Walls[rep].RoofRise;
		FVector Knob = Base + FVector(0, 0, Rise);
		MakeHandle(CornerHandleMesh, Knob, EHandleType::RoofKnob, rep, 0);

		UStaticMeshComponent* BaseCube = MakeHandleDecoration(CornerHandleMesh);
		BaseCube->SetWorldLocation(Base);
		BaseCube->SetWorldScale3D(FVector(HandleScale));

		UStaticMeshComponent* Line = MakeHandleDecoration(CornerHandleMesh);
		Line->SetWorldLocation(Base + FVector(0, 0, Rise * 0.5f));
		FVector Native = CornerHandleMesh->GetBoundingBox().GetSize();
		float Thickness = 5.f;
		Line->SetWorldScale3D(FVector(Thickness / Native.X, Thickness / Native.Y, Rise / Native.Z));
		return;
	}
	if (SelectionType == ESelectionType::Opening)
	{
		FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
		USplineComponent* OSpline = Owner->Walls[SelectedWallIndex].SplineComponent;
		float dc = O.Distance;
		float d0 = dc - O.Width / 2.f;
		float d1 = dc + O.Width / 2.f;
		float MidZ = O.SillHeight + O.OpeningHeight / 2.f;
		FVector Up = FVector::UpVector;
		MakeHandle(MoveHandleMesh,   OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningMove, SelectedWallIndex, 0);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(d0, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningEdgeStart, SelectedWallIndex, 0);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(d1, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningEdgeEnd, SelectedWallIndex, 0);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * (O.SillHeight + O.OpeningHeight), EHandleType::OpeningHead, SelectedWallIndex, 0);
		if (!O.bIsDoor)
			MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * O.SillHeight, EHandleType::OpeningSill, SelectedWallIndex, 0);
		return;
	}

	for (int32 wi : SelectedWalls)
	{
		if (!Owner->Walls.IsValidIndex(wi) || !Owner->Walls[wi].SplineComponent) continue;
		USplineComponent* Spline = Owner->Walls[wi].SplineComponent;
		int32 NumPoints = Spline->GetNumberOfSplinePoints();

		// Corner handles
		for (int32 i = 0; i < NumPoints; i++)
			MakeHandle(CornerHandleMesh, Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), EHandleType::Corner, wi, i);

		// Segment handles
		bool bClosed = Owner->Walls[wi].bClosed;
		int32 NumSeg = bClosed ? NumPoints : NumPoints - 1;
		for (int32 s = 0; s < NumSeg; s++)
		{
			FVector A = Spline->GetLocationAtSplinePoint(s, ESplineCoordinateSpace::World);
			FVector B = Spline->GetLocationAtSplinePoint((s + 1) % NumPoints, ESplineCoordinateSpace::World);
			MakeHandle(MoveHandleMesh, (A + B) * 0.5f, EHandleType::Segment, wi, s);
		}
	}
	// single height slider for the whole group
	if (SelectedWalls.Num() > 0 && Owner->Walls.IsValidIndex(SelectedWalls[0]))
	{
		int32 rep = SelectedWalls[0];
		FVector Base = HeightSliderBase(rep);
		float Height = Owner->Walls[rep].Height;
		FVector Knob = Base + FVector(0, 0, Height);
		MakeHandle(CornerHandleMesh, Knob, EHandleType::HeightKnob, rep, 0);

		UStaticMeshComponent* BaseCube = MakeHandleDecoration(CornerHandleMesh);
		BaseCube->SetWorldLocation(Base);
		BaseCube->SetWorldScale3D(FVector(HandleScale));

		UStaticMeshComponent* Line = MakeHandleDecoration(CornerHandleMesh);
		Line->SetWorldLocation(Base + FVector(0, 0, Height * 0.5f));
		FVector Native = CornerHandleMesh->GetBoundingBox().GetSize();
		float Thickness = 5.f;
		Line->SetWorldScale3D(FVector(Thickness / Native.X, Thickness / Native.Y, Height / Native.Z));
	}
	
}
int32 USelectionAndHandlesComponent::PickHandle()
{
	float MouseX, MouseY;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))return -1;
	FVector2D Mouse(MouseX, MouseY);

	int32 Best = -1;
	float BestDist = HandlePickRadius;
	
	for (int32 i = 0; i < Handles.Num(); i++)
	{
		FVector2D Screen;
		if (!PlayerController->ProjectWorldLocationToScreen(Handles[i]->GetComponentLocation(), Screen))
			continue;
		float Dist = FVector2D::Distance(Mouse, Screen);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}
bool USelectionAndHandlesComponent::GetCursorOnPlane(FVector PlanePoint,FVector PlaneNormal, FVector& Out)
{
	float MouseX, MouseY;
	if (!PlayerController->GetMousePosition(MouseX, MouseY)) return false;

	FVector Origin, Dir;
	if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Dir)) return false;

	float Denom = FVector::DotProduct(Dir, PlaneNormal);
	if (FMath::IsNearlyZero(Denom)) return false;                               

	float T = FVector::DotProduct(PlanePoint - Origin, PlaneNormal) / Denom;
	if (T < 0.f) return false;
	Out = Origin + Dir * T;
	return true;
}
bool USelectionAndHandlesComponent::GetCursorOnAxis(FVector AxisPoint, FVector AxisDir, float& OutDist)
{
	float MouseX, MouseY;
	if (!PlayerController->GetMousePosition(MouseX, MouseY)) return false;

	FVector Origin, Dir;
	if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Dir)) return false;
	
	FVector U = AxisDir.GetSafeNormal();
	FVector V = Dir;
	FVector W0 = AxisPoint - Origin;
	float b = FVector::DotProduct(U, V);
	float Denom = 1.f - b * b;         
	if (FMath::IsNearlyZero(Denom)) return false;  

	float d = FVector::DotProduct(U, W0);
	float e = FVector::DotProduct(V, W0);
	OutDist = (b * e - d) / Denom;          
	return true;
}
bool USelectionAndHandlesComponent::TryBeginHandleDrag()
{
	int32 HandleIndex = PickHandle();
	if (HandleIndex == -1) return false;
	bDragging = true;
	DraggedPointIndex = HandleIndex;
	if (SelectionType == ESelectionType::Wall && HandleWall.IsValidIndex(HandleIndex))
		SelectedWallIndex = HandleWall[HandleIndex];
	
	LastDragPoint = Handles[HandleIndex]->GetComponentLocation();
	return true;
}
bool USelectionAndHandlesComponent::TrySetCornerContext()
{
	int32 h = PickHandle();
	if (!HandleTypes.IsValidIndex(h)) return false;
	if (HandleTypes[h] != EHandleType::Corner) return false;   // only corner handles
	ContextWallIndex  = HandleWall[h];
	ContextPointIndex = HandleLocalIndex[h];
	return true;
	
}
void USelectionAndHandlesComponent::EndHandleDrag()
{
	bool bEndpointCorner = false;
	if (bDragging && SelectionType == ESelectionType::Wall
		&& HandleTypes.IsValidIndex(DraggedPointIndex)
		&& HandleTypes[DraggedPointIndex] == EHandleType::Corner
		&& Owner->Walls.IsValidIndex(SelectedWallIndex)
		&& Owner->Walls[SelectedWallIndex].SplineComponent)
	{
		int32 NumPoints = Owner->Walls[SelectedWallIndex].SplineComponent->GetNumberOfSplinePoints();
		int32 LocalPt = HandleLocalIndex.IsValidIndex(DraggedPointIndex) ? HandleLocalIndex[DraggedPointIndex] : -1;
		bEndpointCorner = (LocalPt == 0 || LocalPt == NumPoints - 1);
	}

	if (Handles.IsValidIndex(DraggedPointIndex))
		Handles[DraggedPointIndex]->SetWorldScale3D(FVector(HandleScale));
	bDragging = false;
	DraggedPointIndex = -1;
	HoveredHandleIndex = -1;

	if (bEndpointCorner)
	{
		Owner->Walls[SelectedWallIndex].Junctions.Empty();
		int32 Survivor = Owner->TryMergeWalls(SelectedWallIndex);
		if (Survivor != -1) SelectedWallIndex = Survivor;
		else Owner->TryMakeTJunction(SelectedWallIndex);
		SelectedWalls = Owner->GetConnectedWalls(SelectedWallIndex);
		RefreshHandles();
		BuildSelectionOutline();
	}
	Owner->FinalizeWallSnap(SelectedWallIndex);
}








