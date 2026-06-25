// AS

#include "Building.h"
#include "BuildingMesh.h"
#include "EnhancedInputComponent.h"
#include "GameHUD.h"
#include "EnhancedInputSubsystems.h"
#include "SelectionAndHandlesComponent.h"
#include "Engine/StaticMesh.h"

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

	GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (!GameHUD) return;
	GameHUD->OnModeChanged.AddDynamic(this, &ABuilding::HandleModeChange);
	GameHUD->OnDeleteRequested.AddDynamic(Selection, &USelectionAndHandlesComponent::DeleteSelected);

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
	
	Walls.Add(FWallData{CurrentSpline, NewMesh});
	FVector FirstPointLocation = CurrentSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	FVector LastPointLocation  = CurrentSpline->GetLocationAtSplinePoint(
						CurrentSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	if (FVector::DistXY(FirstPointLocation, LastPointLocation)< WallCloseTolerance)
	{
		Walls.Last().bClosed = true;
	}
	RebuildWall(Walls.Num()-1);
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
	if (Walls[Index].bClosed)
	{
		BuildFloorMesh(Walls[Index].SplineComponent, M);
		BuildRoofMesh(Walls[Index].SplineComponent, Walls[Index].Height, M);
	}
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
void ABuilding::BuildFloorMesh(const USplineComponent* Spline, UProceduralMeshComponent* Target)
{
	FMeshBuffers B = FBuildingMesh::BuildFloor(Spline, WallStep, FloorZOffset);
	Target->CreateMeshSection(1, B.Vertices, B.Triangles, B.Normals, B.UVs, B.VertexColors, B.Tangents, true);
	if (!FloorMaterial)return;
	Target->SetMaterial(1, FloorMaterial);
	
}
void ABuilding::BuildRoofMesh(const USplineComponent* Spline,const int32 WallHeight, UProceduralMeshComponent* Target)
{
	FMeshBuffers B = FBuildingMesh::BuildRoof(Spline, WallHeight, RoofHeight);
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

	OpeningData.OpeningMesh = OpeningMesh;
	Walls[WallIndex].OpeningData.Add(OpeningData);

	BuildWallMesh(Walls[WallIndex], Walls[WallIndex].WallMesh, false);
}


