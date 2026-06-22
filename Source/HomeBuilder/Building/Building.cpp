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
	
	const float WallStep = 50.f;
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
	const float Step = 50.f;
	const float Length = SplineComponent->GetSplineLength();
	const int32 NumRings = FMath::CeilToInt(Length / Step);
	for (int32 i = 0; i < NumRings; i++)
	{
		float d = i * Step;
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
	Transform.SetScale3D(FVector(Thickness/Native.X, OpeningData.Width/Native.Y, OpeningData.OpeningHeight/Native.Z));
	return Transform;
}
void ABuilding::PlaceOpening(EBuildTool Tool)
{
	float Sill = 0, OpeningHeight;
	switch (Tool)
	{
		case EBuildTool::Door: OpeningHeight = 210.f; break;
		case EBuildTool::Window: OpeningHeight = 120.f; break;
		default: return;
	}

	if (!UpdateMousePosition(false))
	{
		return;
	}
	if (Walls.Num() == 0) return;

	int BestWallIndex = -1;
	float BestDistance = BIG_NUMBER;
	float BestKey = 0.0f;

	for (int32 w = 0; w < Walls.Num(); w++)
	{
		USplineComponent* SplineComponent = Walls[w].SplineComponent;
		if (!SplineComponent) continue;
		float Key = SplineComponent->FindInputKeyClosestToWorldLocation(MousePosition);
		FVector ClosestPoint = SplineComponent->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		float Distance = FVector::Dist(ClosestPoint, MousePosition);
		if (Distance < BestDistance){BestDistance = Distance; BestWallIndex = w; BestKey = Key;}
	}
	if (BestWallIndex == -1) return;

	USplineComponent* SplineComponent = Walls[BestWallIndex].SplineComponent;

	FOpeningData OpeningData;

	OpeningData.Distance = SplineComponent->GetDistanceAlongSplineAtSplineInputKey(BestKey);
	float BaseZ = SplineComponent->GetLocationAtDistanceAlongSpline(OpeningData.Distance, ESplineCoordinateSpace::World).Z;
	float ClickHeight = MousePosition.Z - BaseZ;
	if (Tool == EBuildTool::Window)
	{
		Sill = FMath::Clamp(ClickHeight - OpeningHeight / 2.f, 0.0f, Walls[BestWallIndex].Height - OpeningHeight);
	}
	OpeningData.SillHeight = Sill;
	OpeningData.OpeningHeight = OpeningHeight;


	UStaticMeshComponent* OpeningMesh=  NewObject<UStaticMeshComponent>(this);
	OpeningMesh->SetupAttachment(RootComponent);
	OpeningMesh->RegisterComponent();

	OpeningMesh->SetStaticMesh(Tool == EBuildTool::Door ? DoorMesh : WindowMesh);
	OpeningMesh->SetWorldTransform(TransformMesh(OpeningMesh->GetStaticMesh(), SplineComponent, OpeningData, Walls[BestWallIndex].Thickness));

	OpeningData.OpeningMesh = OpeningMesh;
	Walls[BestWallIndex].OpeningData.Add(OpeningData);

	BuildWallMesh(Walls[BestWallIndex], BestWallIndex);
}
bool ABuilding::FindOpeningAt(const FWallData& WallData, float d, float& OutSill, float& OutHead) const
{
	for (const FOpeningData& OpeningData : WallData.OpeningData)
	{
		float HalfWidth = OpeningData.Width / 2.f;
		if (d >= OpeningData.Distance - HalfWidth && d <= OpeningData.Distance + HalfWidth)
		{
			OutSill = OpeningData.SillHeight;
			OutHead = OpeningData.SillHeight + OpeningData.OpeningHeight;
			return true;
		}
	}
	return false;
}
