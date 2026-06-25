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
					Spline->SetLocationAtSplinePoint(DraggedPointIndex, P, ESplineCoordinateSpace::World, true);
					Owner->RebuildWall(SelectedWallIndex);
					break;
			}
			case EHandleType::Segment:
			{
					FVector P;
					if (!GetCursorOnPlane(FVector(0,0,PlaneZ), FVector::UpVector, P)) return;
					int32 Seg = DraggedPointIndex - NumPoints;
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
					FVector Base = HeightSliderBase();
					float HeightAlong;
					if (!GetCursorOnAxis(Base, FVector::UpVector, HeightAlong)) break;
					Owner->Walls[SelectedWallIndex].Height = FMath::Clamp(HeightAlong, 100.f, 600.f);
					Owner->RebuildWall(SelectedWallIndex);
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
		}
		if (SelectionType == ESelectionType::Opening)
		{
			FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
			if (Owner->OpeningOverlaps(Owner->Walls[SelectedWallIndex], O.Distance, O.Width / 2.f,
				O.SillHeight, O.SillHeight + O.OpeningHeight, SelectedOpeningIndex))
				O = Saved;
			Owner->RebuildWall(SelectedWallIndex);
		}
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
		for (const auto& OpeningData : Owner->Walls[SelectedWallIndex].OpeningData)
		{
			OpeningData.OpeningMesh->DestroyComponent();
		}
		Owner->Walls[SelectedWallIndex].WallMesh->DestroyComponent();
		Owner->Walls[SelectedWallIndex].SplineComponent->DestroyComponent();
		Owner->Walls.RemoveAt(SelectedWallIndex);
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
void USelectionAndHandlesComponent::SelectAtCursor()
{
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
		FOpeningData OpeningData = Owner->Walls[WallIndex].OpeningData[i];
		if (Distance >= OpeningData.Distance - OpeningData.Width / 2 && Distance <= OpeningData.Distance + OpeningData.Width / 2)
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
	float Height = Owner->Walls[SelectedWallIndex].Height;
	float Thickness = Owner->Walls[SelectedWallIndex].Thickness;
	if (SelectionType == ESelectionType::Wall)
	{
		FMeshBuffers B = FBuildingMesh::BuildWallOutline(Owner->Walls[SelectedWallIndex], CamLocal, OutlineThickness, Height,
			OutlineDashTile, ABuilding::WallStep);
		SelectionOutline->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, false);
		
	}
	else if (SelectionType == ESelectionType::Opening)
	{
		FMeshBuffers B = FBuildingMesh::BuildOpeningOutline(Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex],
			Spline, CamLocal, Thickness,OutlineThickness, OutlineDashTile);
		SelectionOutline->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, false);
	}

	SelectionOutline->SetVisibility(true);
	if (OutlineMaterial) SelectionOutline->SetMaterial(0, OutlineMaterial);
}
//Handles
UStaticMeshComponent* USelectionAndHandlesComponent::MakeHandle(UStaticMesh* Mesh, const FVector& Location, EHandleType Type)
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
FVector USelectionAndHandlesComponent::HeightSliderBase() const
{
    USplineComponent* Spline = Owner->Walls[SelectedWallIndex].SplineComponent;
    int32 NumPoints = Spline->GetNumberOfSplinePoints();
    bool  bClosed   = Owner->Walls[SelectedWallIndex].bClosed;
    float HalfThick = Owner->Walls[SelectedWallIndex].Thickness * 0.5f;

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
void USelectionAndHandlesComponent::RefreshHandles()
{
	for (UStaticMeshComponent* H : Handles) H->DestroyComponent();
	Handles.Empty();
	HandleTypes.Empty();
	HoveredHandleIndex = -1;
	for (UStaticMeshComponent* D : HandleDecorations) D->DestroyComponent();
	HandleDecorations.Empty();

	if (SelectionType == ESelectionType::None) return;

	if (SelectionType == ESelectionType::Opening)
	{
		FOpeningData& O = Owner->Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
		USplineComponent* OSpline = Owner->Walls[SelectedWallIndex].SplineComponent;
		float dc = O.Distance;
		float d0 = dc - O.Width / 2.f;
		float d1 = dc + O.Width / 2.f;
		float MidZ = O.SillHeight + O.OpeningHeight / 2.f;
		FVector Up = FVector::UpVector;
		MakeHandle(MoveHandleMesh,   OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningMove);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(d0, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningEdgeStart);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(d1, ESplineCoordinateSpace::World) + Up * MidZ, EHandleType::OpeningEdgeEnd);
		MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * (O.SillHeight + O.OpeningHeight), EHandleType::OpeningHead);
		if (!O.bIsDoor)
			MakeHandle(CornerHandleMesh, OSpline->GetLocationAtDistanceAlongSpline(dc, ESplineCoordinateSpace::World) + Up * O.SillHeight, EHandleType::OpeningSill);
		return;
	}

	USplineComponent* Spline = Owner->Walls[SelectedWallIndex].SplineComponent;
	int32 NumPoints = Spline->GetNumberOfSplinePoints();

	// Corner handles
	for (int32 i = 0; i < NumPoints; i++)
		MakeHandle(CornerHandleMesh, Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), EHandleType::Corner);

	// Segment handles
	bool bClosed = Owner->Walls[SelectedWallIndex].bClosed;
	int32 NumSeg = bClosed ? NumPoints : NumPoints - 1;
	for (int32 s = 0; s < NumSeg; s++)
	{
		FVector A = Spline->GetLocationAtSplinePoint(s, ESplineCoordinateSpace::World);
		FVector B = Spline->GetLocationAtSplinePoint((s + 1) % NumPoints, ESplineCoordinateSpace::World);
		MakeHandle(MoveHandleMesh, (A + B) * 0.5f, EHandleType::Segment);
	}
	//Heigh handles
	FVector Base = HeightSliderBase();
	float Height = Owner->Walls[SelectedWallIndex].Height;
	FVector Knob = Base + FVector(0, 0, Height);
	MakeHandle(CornerHandleMesh, Knob, EHandleType::HeightKnob);

	UStaticMeshComponent* BaseCube = MakeHandleDecoration(CornerHandleMesh);
	BaseCube->SetWorldLocation(Base);
	BaseCube->SetWorldScale3D(FVector(HandleScale));

	UStaticMeshComponent* Line = MakeHandleDecoration(CornerHandleMesh);
	Line->SetWorldLocation(Base + FVector(0, 0, Height * 0.5f));
	FVector Native = CornerHandleMesh->GetBoundingBox().GetSize();
	float Thickness = 5.f;
	Line->SetWorldScale3D(FVector(Thickness / Native.X, Thickness / Native.Y, Height / Native.Z));
	
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
	
	LastDragPoint = Handles[HandleIndex]->GetComponentLocation();
	return true;
}
void USelectionAndHandlesComponent::EndHandleDrag()
{
	if (Handles.IsValidIndex(DraggedPointIndex))
		Handles[DraggedPointIndex]->SetWorldScale3D(FVector(HandleScale));
	bDragging = false;
	DraggedPointIndex = -1;
	HoveredHandleIndex = -1;
}








