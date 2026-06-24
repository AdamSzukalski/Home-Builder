// AS


#include "SelectionAndHandlesComponent.h"
#include "Building.h"
#include "BuildingMesh.h"

// Sets default values for this component's properties
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
	if (SelectionType != ESelectionType::None)
	{
		FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
		if (CameraLocation.Equals(LastCameraLocation, 0.5) && CameraRotation.Equals(LastCameraRotation, 0.1)) return;
		LastCameraLocation = CameraLocation;
		LastCameraRotation = CameraRotation;
		BuildSelectionOutline();
	}
}
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
}
void USelectionAndHandlesComponent::SelectAtCursor()
{
	int32 WallIndex;
	float SplineKey;
	if (!Owner->FindWallAtCursor(WallIndex, SplineKey))
	{
		SelectionType = ESelectionType::None;
		BuildSelectionOutline(); //clears + hides
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








