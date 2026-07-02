// AS

#include "Building.h"
#include "BuildingMesh.h"
#include "EnhancedInputComponent.h"
#include "GameHUD.h"
#include "EnhancedInputSubsystems.h"
#include "SelectionAndHandlesComponent.h"
#include "Engine/StaticMesh.h"
#include "Algo/Reverse.h"
#include "Kismet/GameplayStatics.h"

ABuilding::ABuilding()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent =  CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->bUseAsyncCooking = true;

	OpeningPreview = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpeningPreview"));
	OpeningPreview->SetupAttachment(RootComponent);
	OpeningPreview->RegisterComponent();
	
	OpeningPreview->SetVisibility(false);
	OpeningPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Selection = CreateDefaultSubobject<USelectionAndHandlesComponent>(TEXT("Selection"));
}
void ABuilding::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return;
	EnableInput(PlayerController);
	
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent) return;
	EnhancedInputComponent->BindAction(IA_PlacePoint, ETriggerEvent::Started, this, &ABuilding::PlacePoint);
	EnhancedInputComponent->BindAction(IA_PlacePoint, ETriggerEvent::Completed, Selection, &USelectionAndHandlesComponent::EndHandleDrag);
	EnhancedInputComponent->BindAction(IA_Commit, ETriggerEvent::Completed, this, &ABuilding::DrawingFinished);
	EnhancedInputComponent->BindAction(IA_Cancel, ETriggerEvent::Completed, this, &ABuilding::DrawingCancelled);
	EnhancedInputComponent->BindAction(IA_Context, ETriggerEvent::Started, this, &ABuilding::RightClick);
	EnhancedInputComponent->BindAction(IA_Undo, ETriggerEvent::Started, this, &ABuilding::Undo);
	EnhancedInputComponent->BindAction(IA_Redo, ETriggerEvent::Started, this, &ABuilding::Redo);

	GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (!GameHUD) return;
	GameHUD->OnModeChanged.AddDynamic(this, &ABuilding::HandleModeChange);
	GameHUD->OnDeleteRequested.AddDynamic(Selection, &USelectionAndHandlesComponent::DeleteSelected);
	GameHUD->OnDeleteCornerRequested.AddDynamic(Selection, &USelectionAndHandlesComponent::DeleteHoveredCorner);
	GameHUD->OnUndoRequested.AddDynamic(this, &ABuilding::Undo);
	GameHUD->OnRedoRequested.AddDynamic(this, &ABuilding::Redo);

	if (!PreviewMaterial) return;
	OpeningPreview->SetMaterial(0, PreviewMaterial);
}
void ABuilding::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GameHUD) return;

	OpeningPreview->SetVisibility(false);
	switch (GameHUD->CurrentBuildTool)
	{
		case EBuildTool::Wall: TickWallPreview(); break;
		case EBuildTool::Door:
		case EBuildTool::Window: TickOpeningPreview(); break;
		default: break;
	}
}
void ABuilding::HandleModeChange(EToolMode NewMode)
{
	if (!PlayerController) return;
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (!Subsystem) return;

	if (NewMode != EToolMode::Build)
	{
		bIsDrawingMode = false;
		Subsystem->RemoveMappingContext(IMC_Building);
		SetActorTickEnabled(false);
		return;
	}
	bIsDrawingMode = false;
	Subsystem->AddMappingContext(IMC_Building, 0);
	SetActorTickEnabled(true);
}
void ABuilding::PushUndoState()
{
	UndoStack.Add(TakeSnapshot());
	if (UndoStack.Num() > UndoCap)
		UndoStack.RemoveAt(0);
	RedoStack.Empty();
}
void ABuilding::Undo()
{
	if (UndoStack.Num() == 0) return;
	RedoStack.Add(TakeSnapshot());
	RestoreSnapshot(UndoStack.Pop());
}
void ABuilding::Redo()
{
	if (RedoStack.Num() == 0) return;
	UndoStack.Add(TakeSnapshot());
	RestoreSnapshot(RedoStack.Pop());
}
//Helper to get building from everywhere
ABuilding* ABuilding::GetBuilding(const UObject* WorldContextObject)
{
	return Cast<ABuilding>(UGameplayStatics::GetActorOfClass(WorldContextObject, ABuilding::StaticClass()));
}

//Drawing
bool ABuilding::UpdateMousePosition()
{
	if (!PlayerController) return false;
	
	float MouseX, MouseY;
	PlayerController->GetMousePosition(MouseX, MouseY);

	FVector WorldOrigin, WorldDirection;
	PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldOrigin, WorldDirection);

	FVector StartTrace = WorldOrigin;
	FVector EndTrace = WorldOrigin + WorldDirection * 100000.0f;
	
	FHitResult Hit;
	FCollisionQueryParams CollisionParams;
	if(GameHUD && GameHUD->CurrentBuildTool == EBuildTool::Wall)
		CollisionParams.AddIgnoredActor(this);
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, ECC_Visibility, CollisionParams);
	
	if (bHit)
	{
		MousePosition = Hit.ImpactPoint;
	}
	return bHit;
}
void ABuilding::PlacePoint()
{
	if (!GameHUD) return;
	GameHUD->OnHideCornerContext.Broadcast();
	if (GameHUD->CurrentBuildTool == EBuildTool::None)
    	{
    		if (!Selection->TryBeginHandleDrag()) Selection->SelectAtCursor();
    		return;
    	}
	if (GameHUD->CurrentBuildTool == EBuildTool::Wall)
	{
		if (!UpdateMousePosition()) return;
		bIsDrawingMode = true;
		if (CurrentSpline == nullptr)
		{
			CurrentSpline = NewObject<USplineComponent>(this);
			CurrentSpline->SetupAttachment(RootComponent);
			CurrentSpline->RegisterComponent();
			CurrentSpline->ClearSplinePoints();
			CurrentSpline->AddSplinePoint(MousePosition, ESplineCoordinateSpace::World);
		}
		else
		{
			CurrentSpline->AddSplinePoint(MousePosition, ESplineCoordinateSpace::World);
		}
	}
	else if (GameHUD->CurrentBuildTool == EBuildTool::Door || GameHUD->CurrentBuildTool == EBuildTool::Window)
	{
		PlaceOpening(GameHUD->CurrentBuildTool);
	}
}
void ABuilding::DrawingFinished()
{
	bIsDrawingMode = false;
	if (!CurrentSpline) return;
	
	UProceduralMeshComponent* NewMesh = NewObject<UProceduralMeshComponent>(this);
	NewMesh->SetupAttachment(RootComponent);
	NewMesh->RegisterComponent();
	NewMesh->SetCollisionResponseToAllChannels(ECR_Block);
	NewMesh->bUseAsyncCooking = true;

	PushUndoState();
	Walls.Add(FWallData{CurrentSpline, NewMesh});
	Walls.Last().bRounded = GameHUD->bRoundedWalls;
	FVector FirstPointLocation = CurrentSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector LastPointLocation  = CurrentSpline->GetLocationAtSplinePoint(
						CurrentSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	if (FVector::DistXY(FirstPointLocation, LastPointLocation)< WallCloseTolerance)
	{
		Walls.Last().bClosed = true;
		CurrentSpline->RemoveSplinePoint(CurrentSpline->GetNumberOfSplinePoints() - 1);
		CurrentSpline->SetClosedLoop(true);
	}
	int32 Last = Walls.Num() - 1;
	if (TryMergeWalls(Last) == -1 && !TryMakeTJunction(Last)) RebuildWall(Last);
	CurrentSpline = nullptr;
	MeshComponent->ClearMeshSection(0);
}
void ABuilding::DrawingCancelled()
{
	bIsDrawingMode = false;
	if (!CurrentSpline) return;
	CurrentSpline->DestroyComponent();
	CurrentSpline = nullptr;
	MeshComponent->ClearMeshSection(0);
}
void ABuilding::TickWallPreview()
{
	if (bIsDrawingMode && CurrentSpline && CurrentSpline->GetNumberOfSplinePoints() >= 1)
	{
		UpdateMousePosition();
		CurrentSpline->AddSplinePoint(MousePosition, ESplineCoordinateSpace::World);
		ApplySplineType(CurrentSpline, GameHUD->bRoundedWalls);
		BuildWallMesh(FWallData{CurrentSpline}, MeshComponent, true);
		CurrentSpline->RemoveSplinePoint(CurrentSpline->GetNumberOfSplinePoints() - 1);
	}
}
void ABuilding::TickOpeningPreview()
{
	int32 WallIndex; FOpeningData OpeningData; bool bValid;
	if (!ComputeOpeningAtCursor(GameHUD->CurrentBuildTool, WallIndex, OpeningData, bValid)) return;
	if (!bValid) return;
	UStaticMesh* Mesh = (GameHUD->CurrentBuildTool == EBuildTool::Door) ? DoorMesh : WindowMesh;
	OpeningPreview->SetStaticMesh(Mesh);
	OpeningPreview->SetWorldTransform(FBuildingMesh::TransformMesh(OpeningPreview->GetStaticMesh(),
	Walls[WallIndex].SplineComponent, OpeningData, Walls[WallIndex].Thickness));
	OpeningPreview->SetVisibility(true);
}
void ABuilding::RightClick()
{
	if (Selection->TrySetCornerContext())
		GameHUD->OnCornerContext.Broadcast();
}
//Wall && Floor
bool ABuilding::FindWallAtCursor(int32& OutIndex, float& OutKey)
{
	if (!UpdateMousePosition())
	{
		return false;
	}
	if (Walls.Num() == 0) return false;

	int BestWallIndex = -1;
	float BestDistance = BIG_NUMBER;
	float BestSplineKey = 0.0f;

	for (int32 w = 0; w < Walls.Num(); w++)
	{
		USplineComponent* SplineComponent = Walls[w].SplineComponent;
		if (!SplineComponent) continue;
		float Key = SplineComponent->FindInputKeyClosestToWorldLocation(MousePosition);
		FVector ClosestPoint = SplineComponent->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		float Distance = FVector::DistXY(ClosestPoint, MousePosition);
		if (Distance < BestDistance){BestDistance = Distance; BestWallIndex = w; BestSplineKey = Key;}
	}
	if (BestWallIndex == -1) return false;
	if (BestDistance > Walls[BestWallIndex].Thickness / 2.f + CursorTolerance) return false;

	OutIndex = BestWallIndex;
	OutKey = BestSplineKey;
	return true;
}
void ABuilding::RebuildWall(int32 Index)
{
	ApplySplineType(Walls[Index].SplineComponent, Walls[Index].bRounded);
	UProceduralMeshComponent* M = Walls[Index].WallMesh;
	BuildWallMesh(Walls[Index], M, false);
	for (auto& O : Walls[Index].OpeningData)
	{
		if (!O.OpeningMesh) continue;
		bool bFits = FBuildingMesh::OpeningFits(Walls[Index], O);
		O.OpeningMesh->SetVisibility(bFits);
		if (bFits == false) continue;
		O.OpeningMesh->SetWorldTransform(FBuildingMesh::TransformMesh(O.OpeningMesh->GetStaticMesh(), Walls[Index].SplineComponent,
			O, Walls[Index].Thickness));
	}
	if (Walls[Index].bClosed || FormsTRoom(Index))
	{
		BuildFloorMesh(Walls[Index].SplineComponent, M);
		BuildRoofMesh(Walls[Index].SplineComponent, Walls[Index].Height, Walls[Index].Thickness, Walls[Index].RoofRise, M);
	}
	else
	{
		M->ClearMeshSection(1); 
		M->ClearMeshSection(2); 
	}
}
int32 ABuilding::TryMergeWalls(int32 MovedIndex)
{
	if (!Walls.IsValidIndex(MovedIndex) || !Walls[MovedIndex].SplineComponent || Walls[MovedIndex].bClosed)
		return -1;

	USplineComponent* NewSpline = Walls[MovedIndex].SplineComponent;
	const int32 NewNum = NewSpline->GetNumberOfSplinePoints();
	const FVector NewStart = NewSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	const FVector NewEnd   = NewSpline->GetLocationAtSplinePoint(NewNum - 1, ESplineCoordinateSpace::World);
	
	int32 BestIndex = -1;
	for (int32 w = 0; w < Walls.Num(); w++)
	{
		if (w == MovedIndex || Walls[w].bClosed || !Walls[w].SplineComponent) continue;
		USplineComponent* WSpline = Walls[w].SplineComponent;
		const int32 WNum = WSpline->GetNumberOfSplinePoints();
		const FVector wStart = WSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
		const FVector wEnd = WSpline->GetLocationAtSplinePoint(WNum - 1, ESplineCoordinateSpace::World);

		if (FVector::DistXY(NewStart, wStart) < WallCloseTolerance ||
			FVector::DistXY(NewStart, wEnd) < WallCloseTolerance ||
			FVector::DistXY(NewEnd,   wStart) < WallCloseTolerance ||
			FVector::DistXY(NewEnd,   wEnd) < WallCloseTolerance)
		{
			BestIndex = w;
			break;
		}
	}
	if (BestIndex == -1) return -1;

	FWallData& Keep  = Walls[BestIndex];
	FWallData& Moved = Walls[MovedIndex];
	USplineComponent* KeepSpline = Keep.SplineComponent;
	const FVector wStart = KeepSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	const FVector wEnd   = KeepSpline->GetLocationAtSplinePoint(
		KeepSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	bool bClosesLoop =
		(FVector::DistXY(NewStart, wStart) < WallCloseTolerance && FVector::DistXY(NewEnd, wEnd)   < WallCloseTolerance) ||
		(FVector::DistXY(NewStart, wEnd)   < WallCloseTolerance && FVector::DistXY(NewEnd, wStart) < WallCloseTolerance);
	TArray<FVector> Combined;
	for (int32 i = 0; i < KeepSpline->GetNumberOfSplinePoints(); i++)
		Combined.Add(KeepSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	const bool bWStartIsJoint =
		FMath::Min(FVector::DistXY(wStart, NewStart), FVector::DistXY(wStart, NewEnd)) < WallCloseTolerance;
	if (bWStartIsJoint && !bClosesLoop) Algo::Reverse(Combined);
	if (bClosesLoop)
	{
		TArray<FVector> NewPoints;
		for (int32 i = 0; i < NewNum; i++)
			NewPoints.Add(NewSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
		if (FVector::DistXY(NewStart, wEnd) > FVector::DistXY(NewEnd, wEnd))
			Algo::Reverse(NewPoints);
		for (int32 i = 1; i < NewPoints.Num() - 1; i++)
			Combined.Add(NewPoints[i]);
		Keep.bClosed = true;
	}
	else
	{
		TArray<FVector> NewPoints;
		for (int32 i = 0; i < NewNum; i++)
			NewPoints.Add(NewSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
		const bool bNewStartIsJoint =
		FMath::Min(FVector::DistXY(NewStart, wStart), FVector::DistXY(NewStart, wEnd)) < WallCloseTolerance;
		if (!bNewStartIsJoint) Algo::Reverse(NewPoints);    

		for (int32 i = 1; i < NewPoints.Num(); i++)
			Combined.Add(NewPoints[i]);
		Keep.bClosed = false;
	}
	
	TArray<FVector> OpeningWorld;
	for (const FOpeningData& O : Keep.OpeningData)
		OpeningWorld.Add(KeepSpline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World));
	for (const FOpeningData& O : Moved.OpeningData)
		OpeningWorld.Add(NewSpline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World));

	KeepSpline->ClearSplinePoints();
	for (const FVector& P : Combined)
		KeepSpline->AddSplinePoint(P, ESplineCoordinateSpace::World);
	if (bClosesLoop) KeepSpline->SetClosedLoop(true);
	Keep.OpeningData.Append(Moved.OpeningData);
	for (int32 k = 0; k < Keep.OpeningData.Num(); k++)
	{
		const float Key = KeepSpline->FindInputKeyClosestToWorldLocation(OpeningWorld[k]);
		Keep.OpeningData[k].Distance = KeepSpline->GetDistanceAlongSplineAtSplineInputKey(Key);
	}
	
	if (FVector::DistXY(Combined[0], Combined.Last()) < WallCloseTolerance)
		Keep.bClosed = true;
	
	Moved.SplineComponent->DestroyComponent();
	Moved.WallMesh->DestroyComponent();
	Walls.RemoveAt(MovedIndex);
	FixJunctionsAfterRemove(MovedIndex);

	const int32 Survivor = (MovedIndex < BestIndex) ? BestIndex - 1 : BestIndex;
	RebuildWall(Survivor);
	return Survivor;
}
bool ABuilding::TryMakeTJunction(int32 MovedIndex)
{
	if (!Walls.IsValidIndex(MovedIndex) || !Walls[MovedIndex].SplineComponent) return false;
	USplineComponent* MovedSpline = Walls[MovedIndex].SplineComponent;
	const int32 NumPoints = MovedSpline->GetNumberOfSplinePoints();
	if (NumPoints < 2) return false;

	bool bMadeAny = false;
	for (int32 pi : { 0, NumPoints - 1 })
	{
		const FVector E = MovedSpline->GetLocationAtSplinePoint(pi, ESplineCoordinateSpace::World);
		for (int32 w = 0; w < Walls.Num(); w++)
		{
			if (w == MovedIndex || !Walls[w].SplineComponent) continue;
			USplineComponent* T = Walls[w].SplineComponent;
			const float Key = T->FindInputKeyClosestToWorldLocation(E);
			const FVector C = T->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
			const float td = T->GetDistanceAlongSplineAtSplineInputKey(Key);
			const float Len = T->GetSplineLength();
			if (FVector::DistXY(E, C) < WallCloseTolerance && td > WallCloseTolerance && td < Len - WallCloseTolerance)
			{
				MovedSpline->SetLocationAtSplinePoint(pi, C, ESplineCoordinateSpace::World, true);
				FWallJunction J;
				J.PointIndex = pi;
				J.TargetWall = w;
				J.TargetDistance = td;
				Walls[MovedIndex].Junctions.Add(J);
				bMadeAny = true;
				break;
			}
		}
	}
	if (bMadeAny) RebuildWall(MovedIndex);
	return bMadeAny;
}
void ABuilding::ResolveAllJunctions(int32 SkipWall)
{
	for (int32 w = 0; w < Walls.Num(); w++)
	{
		if (w == SkipWall || Walls[w].Junctions.Num() == 0 || !Walls[w].SplineComponent) continue;
		bool bChanged = false;
		for (const FWallJunction& J : Walls[w].Junctions)
		{
			if (!Walls.IsValidIndex(J.TargetWall) || J.TargetWall == w || !Walls[J.TargetWall].SplineComponent) continue;
			USplineComponent* T = Walls[J.TargetWall].SplineComponent;
			const float td = FMath::Clamp(J.TargetDistance, 0.f, T->GetSplineLength());
			const FVector P = T->GetLocationAtDistanceAlongSpline(td, ESplineCoordinateSpace::World);
			Walls[w].SplineComponent->SetLocationAtSplinePoint(J.PointIndex, P, ESplineCoordinateSpace::World, true);
			bChanged = true;
		}
		if (bChanged) RebuildWall(w);
	}
}
void ABuilding::FixJunctionsAfterRemove(int32 RemovedIndex)
{
	for (FWallData& W : Walls)
		for (int32 j = W.Junctions.Num() - 1; j >= 0; j--)
		{
			if (W.Junctions[j].TargetWall == RemovedIndex)      W.Junctions.RemoveAt(j);
			else if (W.Junctions[j].TargetWall > RemovedIndex)  W.Junctions[j].TargetWall--;
		}
}
bool ABuilding::FormsTRoom(int32 Index) const
{
	const TArray<FWallJunction>& J = Walls[Index].Junctions;
	if (J.Num() < 2 || !Walls[Index].SplineComponent) return false;
	int32 Last = Walls[Index].SplineComponent->GetNumberOfSplinePoints() - 1;
	int32 t0 = -1, t1 = -1;
	for (const FWallJunction& j : J)
	{
		if (j.PointIndex == 0) t0 = j.TargetWall;
		if (j.PointIndex == Last) t1 = j.TargetWall;
	}
	return t0 != -1 && t0 == t1;
}
TArray<int32> ABuilding::GetConnectedWalls(int32 WallIndex)
{
	TArray<int32> Group;
	if (!Walls.IsValidIndex(WallIndex)) return Group;
	TArray<int32> Stack;
	Stack.Add(WallIndex);
	while (Stack.Num() > 0)
	{
		int32 w = Stack.Pop();
		if (Group.Contains(w)) continue;
		Group.Add(w);
		for (const FWallJunction& J : Walls[w].Junctions)        // walls this one is glued to
			if (Walls.IsValidIndex(J.TargetWall)) Stack.Add(J.TargetWall);
		for (int32 o = 0; o < Walls.Num(); o++)                  // walls glued to this one
			if (o != w)
				for (const FWallJunction& J : Walls[o].Junctions)
					if (J.TargetWall == w) { Stack.Add(o); break; }
	}
	return Group;
}
void ABuilding::BuildWallMesh(const FWallData& Wall, UProceduralMeshComponent* Target, bool bPreview)
{
	FMeshBuffers B = FBuildingMesh::BuildWall(Wall, WallStep);
	const bool bCollision = !bPreview;
	Target->CreateMeshSection(0, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, bCollision);
	UMaterialInterface* Material = bPreview? PreviewMaterial : WallMaterial;
	if (!Material) return;
	Target->SetMaterial(0, Material);
}
void ABuilding::ApplySplineType(USplineComponent* Spline, bool bRounded)
{
	ESplinePointType::Type Type = bRounded ? ESplinePointType::Curve : ESplinePointType::Linear;
	for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
		Spline->SetSplinePointType(i, Type, false);
	Spline->UpdateSpline();
}
void ABuilding::DeleteCorner(int32 WallIndex, int32 PointIndex)
{
	if (!Walls.IsValidIndex(WallIndex)) return;
	FWallData& Wall = Walls[WallIndex];
	USplineComponent* Spline = Wall.SplineComponent;
	if (!Spline) return;
	const int32 Num = Spline->GetNumberOfSplinePoints();
	if (PointIndex < 0 || PointIndex >= Num) return;

	const bool bInterior = Wall.bClosed || (PointIndex > 0 && PointIndex < Num - 1);
	if (!bInterior)
	{
		if (Num <= 2)
		{
			for (const FOpeningData& O : Wall.OpeningData) if (O.OpeningMesh) O.OpeningMesh->DestroyComponent();
			if (Wall.WallMesh) Wall.WallMesh->DestroyComponent();
			Spline->DestroyComponent();
			Walls.RemoveAt(WallIndex);
			FixJunctionsAfterRemove(WallIndex);
			return;
		}

		TArray<FVector> OpeningWorld;
		for (const FOpeningData& O : Wall.OpeningData)
			OpeningWorld.Add(Spline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World));
		Spline->RemoveSplinePoint(PointIndex, true);

		if (Wall.bClosed && Spline->GetNumberOfSplinePoints() < 3)
		{
			Wall.bClosed = false;
			Spline->SetClosedLoop(false);
		}

		for (int32 k = 0; k < Wall.OpeningData.Num(); k++)
		{
			float Key = Spline->FindInputKeyClosestToWorldLocation(OpeningWorld[k]);
			Wall.OpeningData[k].Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
		}

		for (int32 j = Wall.Junctions.Num() - 1; j >= 0; j--)
		{
			if (Wall.Junctions[j].PointIndex == PointIndex) Wall.Junctions.RemoveAt(j);
			else if (Wall.Junctions[j].PointIndex > PointIndex) Wall.Junctions[j].PointIndex--;
		}

		RebuildWall(WallIndex);
		return;
	}
	TArray<FVector> Pts;
	for (int32 i = 0; i < Num; i++)
	    Pts.Add(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	
	struct FTmp { FOpeningData Data; FVector World; };
	TArray<FTmp> Ops;
	for (const FOpeningData& O : Wall.OpeningData)
	    Ops.Add({ O, Spline->GetLocationAtDistanceAlongSpline(O.Distance, ESplineCoordinateSpace::World) });
	
	TArray<TArray<FVector>> Sides;
	if (Wall.bClosed)
	{
	    TArray<FVector> Chain;
	    for (int32 i = PointIndex + 1; i < Num; i++) Chain.Add(Pts[i]);
	    for (int32 i = 0; i < PointIndex; i++)       Chain.Add(Pts[i]);
	    Sides.Add(Chain);
	}
	else
	{
	    TArray<FVector> L, R;
	    for (int32 i = 0; i < PointIndex; i++)       L.Add(Pts[i]);
	    for (int32 i = PointIndex + 1; i < Num; i++) R.Add(Pts[i]);
	    Sides.Add(L); Sides.Add(R);
	}

	FWallData Template = Wall;              
	Wall.OpeningData.Empty();                  
	Wall.WallMesh->DestroyComponent();
	Walls.RemoveAt(WallIndex);
	Spline->DestroyComponent();
	FixJunctionsAfterRemove(WallIndex);

	for (TArray<FVector>& Side : Sides)
	{
		if (Side.Num() < 2) continue;         
		const int32 NewIdx = SpawnWall(Side, Template);
		USplineComponent* NS = Walls[NewIdx].SplineComponent;
		for (int32 o = Ops.Num() - 1; o >= 0; o--)
		{
			float Key = NS->FindInputKeyClosestToWorldLocation(Ops[o].World);
			FVector C = NS->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
			if (FVector::DistXY(C, Ops[o].World) < Template.Thickness) 
			{
				FOpeningData O2 = Ops[o].Data;
				O2.Distance = NS->GetDistanceAlongSplineAtSplineInputKey(Key);
				Walls[NewIdx].OpeningData.Add(O2);
				Ops.RemoveAt(o);
			}
		}
		RebuildWall(NewIdx);
	}
	for (FTmp& T : Ops)                   
		if (T.Data.OpeningMesh) T.Data.OpeningMesh->DestroyComponent();
}
void ABuilding::FinalizeWallSnap(int32 Index)
{
	if (!Walls.IsValidIndex(Index)) return;
	if (Walls[Index].bClosed)
		FBuildingMesh::SnapSplineTo90(Walls[Index].SplineComponent);
	RebuildWall(Index);
}
int32 ABuilding::SpawnWall(const TArray<FVector>& WorldPoints, const FWallData& Template)
{
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	Mesh->SetupAttachment(RootComponent);
	Mesh->RegisterComponent();
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->bUseAsyncCooking = true;

	USplineComponent* S = NewObject<USplineComponent>(this);
	S->SetupAttachment(RootComponent);
	S->RegisterComponent();
	S->ClearSplinePoints();
	for (const FVector& P : WorldPoints) S->AddSplinePoint(P, ESplineCoordinateSpace::World);

	FWallData W;
	W.SplineComponent = S;
	W.WallMesh = Mesh;
	W.Height = Template.Height;
	W.Thickness = Template.Thickness;
	W.bRounded = Template.bRounded;
	return Walls.Add(W);   // bClosed defaults false
}
TArray<FWallSnapshot> ABuilding::TakeSnapshot()
{
	TArray<FWallSnapshot> Snapshot;
	for (const FWallData& W : Walls)
	{
		FWallSnapshot S;
		for (int i = 0; i < W.SplineComponent->GetNumberOfSplinePoints(); i++)
		{
			S.Points.Add(W.SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
		}
		for (int i = 0; i < W.OpeningData.Num(); i++)
		{
			FOpeningSnapshot O;
			O.Distance = W.OpeningData[i].Distance;
			O.Width = W.OpeningData[i].Width;
			O.SillHeight = W.OpeningData[i].SillHeight;
			O.OpeningHeight = W.OpeningData[i].OpeningHeight;
			O.bIsDoor = W.OpeningData[i].bIsDoor;
			S.OpeningSnapshot.Add(O);
		}
		S.Junctions = W.Junctions;
		S.Height = W.Height;
		S.Thickness = W.Thickness;
		S.bClosed = W.bClosed;
		S.bRounded = W.bRounded;
		S.RoofRise = W.RoofRise;
		Snapshot.Add(S);
	}
	return Snapshot;
}
void ABuilding::RestoreSnapshot(const TArray<FWallSnapshot>& Snapshot)
{
	while (Walls.Num() > Snapshot.Num())
	{
		for (FOpeningData& O : Walls.Last().OpeningData)
		{
			if (O.OpeningMesh) O.OpeningMesh->DestroyComponent();
		}
		Walls.Last().WallMesh->DestroyComponent();
		Walls.Last().SplineComponent->DestroyComponent();
		Walls.RemoveAt(Walls.Num() - 1);
	}
	while (Walls.Num() < Snapshot.Num())
	{
		FWallData W;
		SpawnWall(Snapshot[Walls.Num()].Points, W);
	}
	for (int i = 0; i < Snapshot.Num(); i++)
	{
		Walls[i].SplineComponent->ClearSplinePoints();
		for (int j = 0; j < Snapshot[i].Points.Num(); j++)
		{
			Walls[i].SplineComponent->AddSplinePoint(Snapshot[i].Points[j], ESplineCoordinateSpace::World);
		}
		ApplySplineType(Walls[i].SplineComponent, Snapshot[i].bRounded);

		Walls[i].Junctions = Snapshot[i].Junctions;
		Walls[i].Height = Snapshot[i].Height;
		Walls[i].Thickness = Snapshot[i].Thickness;
		Walls[i].bClosed = Snapshot[i].bClosed;
		Walls[i].bRounded = Snapshot[i].bRounded;
		Walls[i].RoofRise = Snapshot[i].RoofRise;
		
		for (FOpeningData& O : Walls[i].OpeningData)
		{
			if (O.OpeningMesh) O.OpeningMesh->DestroyComponent();
		}
		Walls[i].OpeningData.Empty();

		for (const FOpeningSnapshot& SavedOpening : Snapshot[i].OpeningSnapshot)
		{
			FOpeningData NewOpening;
			NewOpening.Distance = SavedOpening.Distance;
			NewOpening.Width = SavedOpening.Width;
			NewOpening.SillHeight = SavedOpening.SillHeight;
			NewOpening.OpeningHeight = SavedOpening.OpeningHeight;
			NewOpening.bIsDoor = SavedOpening.bIsDoor;

			UStaticMeshComponent* OpeningMesh = NewObject<UStaticMeshComponent>(this);
			OpeningMesh->SetupAttachment(RootComponent);
			OpeningMesh->RegisterComponent();
			OpeningMesh->SetStaticMesh(NewOpening.bIsDoor ? DoorMesh : WindowMesh);
			OpeningMesh->SetWorldTransform(FBuildingMesh::TransformMesh(OpeningMesh->GetStaticMesh(),
				Walls[i].SplineComponent, NewOpening, Walls[i].Thickness));

			NewOpening.OpeningMesh = OpeningMesh;
			Walls[i].OpeningData.Add(NewOpening);
		}

		RebuildWall(i);
	}
}
void ABuilding::BuildFloorMesh(const USplineComponent* Spline, UProceduralMeshComponent* Target)
{
	FMeshBuffers B = FBuildingMesh::BuildFloor(Spline, WallStep, FloorZOffset);
	Target->CreateMeshSection(1, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, true);
	if (!FloorMaterial)return;
	Target->SetMaterial(1, FloorMaterial);
	
}
void ABuilding::BuildRoofMesh(const USplineComponent* Spline,const int32 WallHeight, const int32 WallThickness, float RoofRise, UProceduralMeshComponent* Target)
{
	FMeshBuffers B = FBuildingMesh::BuildRoof(Spline, WallHeight, RoofRise, WallThickness * 0.5f);
	Target->CreateMeshSection(2, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, true);
	if (!RoofMaterial)return;
	Target->SetMaterial(2, RoofMaterial);
}
//Doors && Windows
bool ABuilding::ComputeOpeningAtCursor(EBuildTool Tool, int32& OutWallIndex, FOpeningData& OutOpeningData, bool& bValid)
{
	float Sill = 0, OpeningHeight;
	switch (Tool)
	{
		case EBuildTool::Door: OpeningHeight = DoorHeight; break;
		case EBuildTool::Window: OpeningHeight = WindowHeight; break;
		default: return false;
	}
	
	int32 BestWallIndex = -1;
	float BestSplineKey = 0.0f;
	
	if (!FindWallAtCursor(BestWallIndex, BestSplineKey)) return false;

	USplineComponent* SplineComponent = Walls[BestWallIndex].SplineComponent;
	FOpeningData OpeningData;
	OpeningData.Distance = SplineComponent->GetDistanceAlongSplineAtSplineInputKey(BestSplineKey);
	
	//Make it not exit the wall
	float Length = SplineComponent->GetSplineLength();
	float HalfWidth = OpeningData.Width / 2.f;
	if (Length < OpeningData.Width) return false;
	
	OpeningData.Distance = FMath::Clamp(OpeningData.Distance, HalfWidth, Length - HalfWidth);
	
	float BaseZ = SplineComponent->GetLocationAtDistanceAlongSpline(OpeningData.Distance, ESplineCoordinateSpace::World).Z;
	float ClickHeight = MousePosition.Z - BaseZ;
	if (Tool == EBuildTool::Window)
	{
		Sill = FMath::Clamp(ClickHeight - OpeningHeight / 2.f, 0.0f, Walls[BestWallIndex].Height - OpeningHeight);
	}
	OpeningData.SillHeight = Sill;
	OpeningData.OpeningHeight = OpeningHeight;
	//Make the Openings not Overlap
	bValid = !OpeningOverlaps(Walls[BestWallIndex], OpeningData.Distance, HalfWidth, OpeningData.SillHeight,
		OpeningData.SillHeight + OpeningData.OpeningHeight);
	OutWallIndex = BestWallIndex;
	OutOpeningData = OpeningData;
	return true;
}
bool ABuilding::OpeningOverlaps(const FWallData& Wall, float Distance, float HalfWidth, float Sill, float Head, int32 IgnoreIndex) const
{
	for (int i = 0; i < Wall.OpeningData.Num(); i++)
	{
		if (i == IgnoreIndex) continue;
		const FOpeningData& O = Wall.OpeningData[i];
		if (FMath::Abs(Distance - O.Distance) < HalfWidth + O.Width / 2.f + OpeningGap &&
		Sill < (O.SillHeight + O.OpeningHeight) + OpeningGap && O.SillHeight < Head + OpeningGap)
			return true;
	}
	return false;
}
void ABuilding::PlaceOpening(EBuildTool Tool)
{
	int32 WallIndex;
	FOpeningData OpeningData;
	bool bValid;
	if (!ComputeOpeningAtCursor(Tool, WallIndex, OpeningData, bValid) || !bValid) return;
	OpeningData.bIsDoor = (Tool == EBuildTool::Door);

	UStaticMeshComponent* OpeningMesh=  NewObject<UStaticMeshComponent>(this);
	OpeningMesh->SetupAttachment(RootComponent);
	OpeningMesh->RegisterComponent();

	OpeningMesh->SetStaticMesh(Tool == EBuildTool::Door ? DoorMesh : WindowMesh);
	OpeningMesh->SetWorldTransform(FBuildingMesh::TransformMesh(OpeningMesh->GetStaticMesh(),
		Walls[WallIndex].SplineComponent, OpeningData, Walls[WallIndex].Thickness));

	PushUndoState();
	OpeningData.OpeningMesh = OpeningMesh;
	Walls[WallIndex].OpeningData.Add(OpeningData);

	BuildWallMesh(Walls[WallIndex], Walls[WallIndex].WallMesh, false);
}


