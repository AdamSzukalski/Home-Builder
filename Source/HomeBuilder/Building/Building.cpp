// AS

#include "EnhancedInputComponent.h"
#include "GameHUD.h"
#include "KismetProceduralMeshLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Building.h"

ABuilding::ABuilding()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent =  CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->bUseAsyncCooking = true;
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
	EnhancedInputComponent->BindAction(IA_Commit, ETriggerEvent::Completed, this, &ABuilding::DrawingFinished);
	EnhancedInputComponent->BindAction(IA_Cancel, ETriggerEvent::Completed, this, &ABuilding::DrawingCancelled);

	GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (!GameHUD) return;
	GameHUD->OnModeChanged.AddDynamic(this, &ABuilding::HandleModeChange);
	GameHUD->OnDeleteRequested.AddDynamic(this, &ABuilding::DeleteSelected);

	OpeningPreview = NewObject<UStaticMeshComponent>(this);
	OpeningPreview->SetupAttachment(RootComponent);
	OpeningPreview->RegisterComponent();

	if (!PreviewMaterial) return;
	OpeningPreview->SetMaterial(0, PreviewMaterial);
	OpeningPreview->SetVisibility(false);
	OpeningPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SelectionOutline = NewObject<UProceduralMeshComponent>(this);
	SelectionOutline->SetupAttachment(RootComponent);
	SelectionOutline->RegisterComponent();
	
	SelectionOutline->SetVisibility(false);
	SelectionOutline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABuilding::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDrawingMode && CurrentSpline && CurrentSpline->GetNumberOfSplinePoints() >= 1)
	{
		UpdateMousePosition(true);
		
		CurrentSpline->AddSplinePoint(MousePosition, ESplineCoordinateSpace::World);
		BuildWallMesh(FWallData{ CurrentSpline }, WallPreviewSectionIndex);
		CurrentSpline->RemoveSplinePoint(CurrentSpline->GetNumberOfSplinePoints() - 1);
	}
	if (!GameHUD) return;
	if (GameHUD->CurrentBuildTool == EBuildTool::Door || GameHUD->CurrentBuildTool == EBuildTool::Window)
	{
		int32 WallIndex;
		FOpeningData OpeningData;
		bool bValid;
		if (!ComputeOpeningAtCursor(GameHUD->CurrentBuildTool, WallIndex, OpeningData, bValid))
			OpeningPreview->SetVisibility(false);
		else if (bValid)
		{
			UStaticMesh* Mesh = (GameHUD->CurrentBuildTool == EBuildTool::Door) ? DoorMesh : WindowMesh;
			OpeningPreview->SetStaticMesh(Mesh);
			OpeningPreview->SetWorldTransform(TransformMesh(OpeningPreview->GetStaticMesh(),
				Walls[WallIndex].SplineComponent, OpeningData, Walls[WallIndex].Thickness));
			OpeningPreview->SetVisibility(true);
		}
	}
	else
	{
		OpeningPreview->SetVisibility(false);
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
void ABuilding::DeleteSelected()
{
	
}
//Selection
void ABuilding::SelectAtCursor()
{
	int32 WallIndex;
	float SplineKey;
	if (!FindWallAtCursor(WallIndex, SplineKey))
	{
		SelectionType = ESelectionType::None;
		BuildSelectionOutline(); //clears + hides
		return;
	}
	USplineComponent* Spline = Walls[WallIndex].SplineComponent;
	float Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(SplineKey);
	bool bFound = false;
	for (int i = 0; i < Walls[WallIndex].OpeningData.Num(); i++)
	{
		FOpeningData OpeningData = Walls[WallIndex].OpeningData[i];
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
}

void ABuilding::BuildSelectionOutline()
{
	SelectionOutline->ClearMeshSection(0);
	
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	
	auto BuildDashedRibbon = [&](const TArray<FVector>& Points, FVector FaceNormal)
	{
		int32 N = Points.Num();
		for (int32 e = 0; e < N; e++)
		{
			FVector A = Points[e];
			FVector B = Points[(e + 1) % N];
			FVector Direction = (B - A).GetSafeNormal();
			FVector Perpendicular = FVector::CrossProduct(FaceNormal, Direction);
			float Len = (B - A).Size();
			float UEnd = Len / SelectionOutlineDashTile;

			int32 base = Vertices.Num();
			Vertices.Add(A - Perpendicular*SelectionOutlineThickness); UVs.Add(FVector2D(0,0));
			Vertices.Add(A + Perpendicular*SelectionOutlineThickness); UVs.Add(FVector2D(0,1));
			Vertices.Add(B - Perpendicular*SelectionOutlineThickness); UVs.Add(FVector2D(UEnd,0));
			Vertices.Add(B + Perpendicular*SelectionOutlineThickness); UVs.Add(FVector2D(UEnd,1));

			Triangles.Add(base+0); Triangles.Add(base+1); Triangles.Add(base+2);
			Triangles.Add(base+2);Triangles.Add(base+1);Triangles.Add(base+3);
		}
	};
	if (SelectionType == ESelectionType::None)
	{
		SelectionOutline->ClearMeshSection(0);
		SelectionOutline->SetVisibility(false);
		return;
	}
	USplineComponent* SplineComponent = Walls[SelectedWallIndex].SplineComponent;
	if (SelectionType == ESelectionType::Wall)
	{
		TArray<FVector> Footprint;
		float Length = SplineComponent->GetSplineLength();
		int32 Num = FMath::CeilToInt(Length / WallStep);
		for (int32 i = 0; i < Num; i++)
			Footprint.Add(SplineComponent->GetLocationAtDistanceAlongSpline(i * WallStep, ESplineCoordinateSpace::Local));
		BuildDashedRibbon(Footprint, FVector::UpVector);
		TArray<FVector> Top = Footprint;
		for (FVector& P : Top) P.Z += Walls[SelectedWallIndex].Height;
		BuildDashedRibbon(Top, FVector::UpVector);
	}
	else if (SelectionType == ESelectionType::Opening)
	{
		FOpeningData OpeningData = Walls[SelectedWallIndex].OpeningData[SelectedOpeningIndex];
		float d0 = OpeningData.Distance - OpeningData.Width / 2;
		float d1 = OpeningData.Distance + OpeningData.Width / 2;

		FVector L0 = SplineComponent->GetLocationAtDistanceAlongSpline(d0, ESplineCoordinateSpace::Local);
		FVector L1 = SplineComponent->GetLocationAtDistanceAlongSpline(d1, ESplineCoordinateSpace::Local);

		float SillHeight = OpeningData.SillHeight;
		float HeadHeight = OpeningData.SillHeight + OpeningData.OpeningHeight;
		float HalfThickness = Walls[SelectedWallIndex].Thickness / 2;
		FVector Up = FVector::UpVector;
		FVector r0 = SplineComponent->GetRightVectorAtDistanceAlongSpline(d0, ESplineCoordinateSpace::Local);
		FVector r1 = SplineComponent->GetRightVectorAtDistanceAlongSpline(d1, ESplineCoordinateSpace::Local);

		FVector C0 = (L0 + Up*SillHeight) + r0 * HalfThickness;
		FVector C1 = (L1 + Up*SillHeight) + r1 * HalfThickness;
		FVector C2 = (L1 + Up*HeadHeight) + r1 * HalfThickness;
		FVector C3 = (L0 + Up*HeadHeight) + r0 * HalfThickness;
		
		TArray Corners = {C0, C1, C2, C3};
		FVector FaceNormal = r0;
		BuildDashedRibbon(Corners, FaceNormal);
	}
	SelectionOutline->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
	SelectionOutline->SetVisibility(true);
	if (SelectionOutlineMaterial) SelectionOutline->SetMaterial(0, SelectionOutlineMaterial);
}


//Drawing
bool ABuilding::UpdateMousePosition(bool bSnap)
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
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, ECC_Visibility, CollisionParams);
	
	if (bHit)
	{
		if (bSnap)
			MousePosition = FVector(
				FMath::GridSnap(Hit.ImpactPoint.X, SnapGridSize),
				FMath::GridSnap(Hit.ImpactPoint.Y, SnapGridSize),
				Hit.ImpactPoint.Z);
		else
			MousePosition = Hit.ImpactPoint;
	}
	return bHit;
}
void ABuilding::PlacePoint()
{
	if (!GameHUD) return;
	bIsDrawingMode = true;
	if (!UpdateMousePosition(true)) return;
	if (GameHUD->CurrentBuildTool == EBuildTool::Wall)
	{
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
	else if (GameHUD->CurrentBuildTool == EBuildTool::None){SelectAtCursor(); return;}
}
void ABuilding::DrawingFinished()
{
	bIsDrawingMode = false;
	if (!CurrentSpline) return;
	Walls.Add(FWallData{CurrentSpline});
	BuildWallMesh(Walls.Last(), Walls.Num()-1);
	FVector FirstPointLocation = CurrentSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector LastPointLocation  = CurrentSpline->GetLocationAtSplinePoint(
						CurrentSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	if (FVector::DistXY(FirstPointLocation, LastPointLocation)< SnapGridSize)
	{
		BuildFloorMesh(CurrentSpline, 2000 + (Walls.Num()-1));
		BuildRoofMesh(CurrentSpline, Walls.Last().Height, 3000 + (Walls.Num()-1));
	}
	CurrentSpline = nullptr;
	MeshComponent->ClearMeshSection(WallPreviewSectionIndex);
}
void ABuilding::DrawingCancelled()
{
	bIsDrawingMode = false;
	if (!CurrentSpline) return;
	CurrentSpline->DestroyComponent();
	CurrentSpline = nullptr;
	MeshComponent->ClearMeshSection(WallPreviewSectionIndex);
}
//Wall && Floor
bool ABuilding::FindWallAtCursor(int32& OutIndex, float& OutKey)
{
	if (!UpdateMousePosition(false))
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
	if (BestDistance > Walls[BestWallIndex].Thickness / 2.f + /*Tolerance*/ 20.f) return false;

	OutIndex = BestWallIndex;
	OutKey = BestSplineKey;
	return true;
}

void ABuilding::BuildWallMesh(const FWallData& Wall, int32 SectionIndex)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	const USplineComponent* SplineComponent = Wall.SplineComponent;
	if (!SplineComponent) return;
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

		Vertices.Add(Location - Right*HalfThickness);//X1
		Vertices.Add(Location + Right*HalfThickness);//X2
		Vertices.Add(Location - Right*HalfThickness + FVector::UpVector*Height);//Y1
		Vertices.Add(Location + Right*HalfThickness + FVector::UpVector*Height);//Y2
	}
	
	auto AddQuad = [&](int32 A, int32 B, int32 C, int32 D)
	{
		Triangles.Add(A); Triangles.Add(B); Triangles.Add(C);
		Triangles.Add(C); Triangles.Add(B); Triangles.Add(D);
	};
	auto AddPanel = [&](FVector La, FVector Ra, FVector Lb, FVector Rb,
					float zLow, float zHigh, bool bTopCap, bool bBottomCap)
	{
		const FVector Up = FVector::UpVector;
		int32 b = Vertices.Num();
		Vertices.Add(La + Up*zLow);   // b+0 LA_low
		Vertices.Add(Ra + Up*zLow);   // b+1 RA_low
		Vertices.Add(La + Up*zHigh);  // b+2 LA_high
		Vertices.Add(Ra + Up*zHigh);  // b+3 RA_high
		Vertices.Add(Lb + Up*zLow);   // b+4 LB_low
		Vertices.Add(Rb + Up*zLow);   // b+5 RB_low
		Vertices.Add(Lb + Up*zHigh);  // b+6 LB_high
		Vertices.Add(Rb + Up*zHigh);  // b+7 RB_high

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

		FVector La = Vertices[r0+0], Ra = Vertices[r0+1];  // ground corners (your precomputed bottom verts)
		FVector Lb = Vertices[r1+0], Rb = Vertices[r1+1];

		float Sill, Head;
		if (FindOpeningAt(Wall, dMid, Sill, Head))
		{
			AddPanel(La, Ra, Lb, Rb, 0.f,  Sill,   false,  false); // below: top cap = windowsill
			AddPanel(La, Ra, Lb, Rb, Head, Height, true,  false);  // above: top = wall top, bottom = lintel
		}
		else
		{
			AddPanel(La, Ra, Lb, Rb, 0.f, Height, true, false);   // solid: full height
		}
	}
	const int32 LastRing = (Samples.Num() - 1) *4;
	AddQuad(0,1,2,3);
	AddQuad(LastRing, LastRing+2, LastRing+1, LastRing+3);
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	if (SectionIndex == WallPreviewSectionIndex)
	{
		MeshComponent->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
		if (PreviewMaterial)
			MeshComponent->SetMaterial(SectionIndex, PreviewMaterial);
	}
	else
	{
		MeshComponent->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
		if (WallMaterial)
			MeshComponent->SetMaterial(SectionIndex, WallMaterial);
	}
}
void ABuilding::BuildFloorMesh(const USplineComponent* SplineComponent, int32 SectionIndex)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	
	if (!SplineComponent) return;
	const int32 N = SplineComponent->GetNumberOfSplinePoints();
	if (N < 3) return;
	
	TArray<FVector> Outline;
	const float Length = SplineComponent->GetSplineLength();
	const int32 NumRings = FMath::CeilToInt(Length / WallStep);
	for (int32 i = 0; i < NumRings; i++)
	{
		float d = i * WallStep;
		Outline.Add(SplineComponent->GetLocationAtDistanceAlongSpline(d, ESplineCoordinateSpace::Local));
	}
	
	float FloorZ = Outline[0].Z;
	for (const FVector& P : Outline) FloorZ = FMath::Max(FloorZ, P.Z);
	FloorZ += 2.f;
	for (FVector& P : Outline) P.Z = FloorZ;
	
	FVector Center = FVector::ZeroVector;
	for (const FVector& P : Outline) Center += P;
	Center /= Outline.Num();
	
	for (const FVector& P : Outline) Vertices.Add(P);
	Vertices.Add(Center);

	const int32 M = Outline.Num();
	for (int32 i = 0; i < M; i++)
	{
		Triangles.Add(M);
		Triangles.Add((i + 1) % M);
		Triangles.Add(i);
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	MeshComponent->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
	if (FloorMaterial)
		MeshComponent->SetMaterial(SectionIndex, FloorMaterial);
	
}

void ABuilding::BuildRoofMesh(const USplineComponent* SplineComponent,const int32 WallHeight, int32 SectionIndex)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	
	if (!SplineComponent) return;
	const int32 N = SplineComponent->GetNumberOfSplinePoints();
	if (N < 3) return;
	
	TArray<FVector> P;
	for (int i = 0; i < N; i++)
	{
		P.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
		P[i].Z += WallHeight;
	}
	FVector RidgeA, RidgeB;
	FVector Up = FVector::UpVector;
	if ((P[0] - P[1]).Size() > (P[1] - P[2]).Size())
	{
		RidgeA = FMath::Lerp(P[1], P[2], 0.5f) + Up * RoofHeight;
		RidgeB = FMath::Lerp(P[3], P[0], 0.5f) + Up * RoofHeight;

		//Roof Planes
		Vertices.Add(P[0]);
		Vertices.Add(P[1]);
		Vertices.Add(RidgeA);
		Vertices.Add(RidgeB);

		Vertices.Add(P[2]);
		Vertices.Add(P[3]);
		Vertices.Add(RidgeB);
		Vertices.Add(RidgeA);

		//Roof Ends
		Vertices.Add(P[1]);
		Vertices.Add(P[2]);
		Vertices.Add(RidgeA);
		Vertices.Add(P[3]);
		Vertices.Add(P[0]);
		Vertices.Add(RidgeB);
	}
	else
	{
		RidgeA = FMath::Lerp(P[0], P[1], 0.5f) + Up * RoofHeight;
		RidgeB = FMath::Lerp(P[2], P[3], 0.5f) + Up * RoofHeight;

		//Roof Planes
		Vertices.Add(P[1]);
		Vertices.Add(P[2]);
		Vertices.Add(RidgeA);
		Vertices.Add(RidgeB);

		Vertices.Add(P[3]);
		Vertices.Add(P[0]);
		Vertices.Add(RidgeB);
		Vertices.Add(RidgeA);

		//Roof Ends
		Vertices.Add(P[0]);
		Vertices.Add(P[1]);
		Vertices.Add(RidgeA);
		Vertices.Add(P[2]);
		Vertices.Add(P[3]);
		Vertices.Add(RidgeB);
	}
	// Roof planes
	for (int32 base : {0, 4})
	{
		Triangles.Add(base+0); Triangles.Add(base+2); Triangles.Add(base+1);
		Triangles.Add(base+0); Triangles.Add(base+3); Triangles.Add(base+2);
	}
	// Roof ends
	for (int32 base : {8, 11})
	{
		Triangles.Add(base+0); Triangles.Add(base+2); Triangles.Add(base+1);
	}
		
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	MeshComponent->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
	if (RoofMaterial)
		MeshComponent->SetMaterial(SectionIndex, RoofMaterial);
}

//Doors && Windows
FTransform ABuilding::TransformMesh(const UStaticMesh* StaticMesh, const USplineComponent* SplineComponent,
		const FOpeningData& OpeningData, float Thickness)
{
	FTransform Transform;

	Transform = SplineComponent->GetTransformAtDistanceAlongSpline(OpeningData.Distance, ESplineCoordinateSpace::World);
	const FVector Up = FVector::UpVector;
	Transform.SetLocation(Transform.GetLocation() + Up*(OpeningData.SillHeight + OpeningData.OpeningHeight / 2));
	FRotator Rotator = Transform.Rotator();
	Rotator.Yaw += 90.f;
	Transform.SetRotation(Rotator.Quaternion());
	
	FVector Native = StaticMesh->GetBounds().BoxExtent * 2.f; 
	Transform.SetScale3D(FVector((Thickness + 2.f)/Native.X, OpeningData.Width/Native.Y, OpeningData.OpeningHeight/Native.Z));
	return Transform;
}
bool ABuilding::ComputeOpeningAtCursor(EBuildTool Tool, int32& OutWallIndex, FOpeningData& OutOpeningData, bool& bValid)
{
	float Sill = 0, OpeningHeight;
	switch (Tool)
	{
		case EBuildTool::Door: OpeningHeight = 210.f; break;
		case EBuildTool::Window: OpeningHeight = 120.f; break;
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
	
	//Make the Openings not Overlap
	bValid = true;
	float Gap = 8.f;
	for (int32 i = 0; i < Walls[BestWallIndex].OpeningData.Num(); i++)
	{
		for (const FOpeningData& O : Walls[BestWallIndex].OpeningData)
		{
			
			float OHalf = O.Width /2.f;
			float MinCenterToCenter = HalfWidth + OHalf + Gap;
			if (FMath::Abs(OpeningData.Distance - O.Distance) < MinCenterToCenter)
			{
				if (OpeningData.Distance < O.Distance)
					OpeningData.Distance = O.Distance - MinCenterToCenter;
				else
					OpeningData.Distance = O.Distance + MinCenterToCenter;
			
			}
		}
		OpeningData.Distance = FMath::Clamp(OpeningData.Distance, HalfWidth, Length - HalfWidth);
	}
	float NewStart = OpeningData.Distance - HalfWidth;
	float NewEnd = OpeningData.Distance + HalfWidth;
	for (const FOpeningData& O : Walls[BestWallIndex].OpeningData)
	{
			
		float OHalf = O.Width /2.f;
		if (NewStart < O.Distance + (OHalf + Gap) && NewEnd > O.Distance - (OHalf + Gap))
		{
			bValid = false;
			break;
		}
	}
	
	
	float BaseZ = SplineComponent->GetLocationAtDistanceAlongSpline(OpeningData.Distance, ESplineCoordinateSpace::World).Z;
	float ClickHeight = MousePosition.Z - BaseZ;
	if (Tool == EBuildTool::Window)
	{
		Sill = FMath::Clamp(ClickHeight - OpeningHeight / 2.f, 0.0f, Walls[BestWallIndex].Height - OpeningHeight);
	}
	OpeningData.SillHeight = Sill;
	OpeningData.OpeningHeight = OpeningHeight;

	OutWallIndex = BestWallIndex;
	OutOpeningData = OpeningData;
	return true;
}

void ABuilding::PlaceOpening(EBuildTool Tool)
{
	int32 WallIndex;
	FOpeningData OpeningData;
	bool bValid;
	if (!ComputeOpeningAtCursor(Tool, WallIndex, OpeningData, bValid)) return;

	UStaticMeshComponent* OpeningMesh=  NewObject<UStaticMeshComponent>(this);
	OpeningMesh->SetupAttachment(RootComponent);
	OpeningMesh->RegisterComponent();

	OpeningMesh->SetStaticMesh(Tool == EBuildTool::Door ? DoorMesh : WindowMesh);
	OpeningMesh->SetWorldTransform(TransformMesh(OpeningMesh->GetStaticMesh(),
		Walls[WallIndex].SplineComponent, OpeningData, Walls[WallIndex].Thickness));

	OpeningData.OpeningMesh = OpeningMesh;
	Walls[WallIndex].OpeningData.Add(OpeningData);

	BuildWallMesh(Walls[WallIndex], WallIndex);
}
bool ABuilding::FindOpeningAt(const FWallData& WallData, float d, float& OutSill, float& OutHead) const
{
	for (const FOpeningData& O : WallData.OpeningData)
	{
		float HalfWidth = O.Width / 2.f;
		if (d >= O.Distance - HalfWidth && d <= O.Distance + HalfWidth)
		{
			OutSill = O.SillHeight;
			OutHead = O.SillHeight + O.OpeningHeight;
			return true;
		}
	}
	return false;
}

