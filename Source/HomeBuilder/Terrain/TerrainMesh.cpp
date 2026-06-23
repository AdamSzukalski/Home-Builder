// AS

#include "TerrainMesh.h"
#include "KismetProceduralMeshLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/DecalComponent.h"
#include "GameTypes.h"
#include "GameHUD.h"

ATerrainMesh::ATerrainMesh()
{
	PrimaryActorTick.bCanEverTick = true;
	
	MeshComponent =  CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->bUseAsyncCooking = true;

	BrushDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BrushDecal"));
	BrushDecal->SetupAttachment(RootComponent);
	
}

void ATerrainMesh::BeginPlay()
{
	Super::BeginPlay();
	
	GenerateTerrain();
	
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, /*bCreateCollision=*/true);
	if (!Material) return;
	MeshComponent->SetMaterial(0, Material);

	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController) return;
	EnableInput(PlayerController);
	
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent) return;
	EnhancedInputComponent->BindAction(IA_Sculpt, ETriggerEvent::Started, this, &ATerrainMesh::SculptStarted);
	EnhancedInputComponent->BindAction(IA_Sculpt, ETriggerEvent::Completed, this, &ATerrainMesh::SculptFinished);
	EnhancedInputComponent->BindAction(IA_Sculpt, ETriggerEvent::Canceled, this, &ATerrainMesh::SculptFinished);

	EnhancedInputComponent->BindAction(IA_Paint, ETriggerEvent::Started, this, &ATerrainMesh::PaintStarted);
	EnhancedInputComponent->BindAction(IA_Paint, ETriggerEvent::Completed, this, &ATerrainMesh::PaintFinished);
	EnhancedInputComponent->BindAction(IA_Paint, ETriggerEvent::Canceled, this, &ATerrainMesh::PaintFinished);
	
	GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (!GameHUD) return;
	GameHUD->OnModeChanged.AddDynamic(this, &ATerrainMesh::HandleModeChange);
	GameHUD->SetMode(EToolMode::Terrain);

	if (!BrushDecal) return;
	if (!BrushMaterial) return;
	BrushDecal->SetMaterial(0, BrushMaterial);
}

void ATerrainMesh::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!BrushDecal || 	!GameHUD) return;
	if (GameHUD->CurrentTerrainTool == ETerrainTool::None)
	{
		BrushDecal->SetVisibility(false);
		return;
	}
	bool bHit = UpdateBrushPosition();
	BrushDecal->SetVisibility(bHit);
	if (bHit)
	{
		BrushDecal->SetWorldLocation(BrushCenter);
		BrushDecal->SetWorldRotation(FRotationMatrix::MakeFromX(BrushNormal).Rotator());
		const float WorldRadius = GameHUD->BrushSize * QuadSize;
		BrushDecal->DecalSize = FVector(ProjectionDepth, WorldRadius, WorldRadius);
	}
	
	int32 TexelX, TexelY;
	WorldToTexel(BrushCenter, TexelX, TexelY);
	//Sculpting
	if (bIsSculptingMode)
	{
		if (GameHUD->CurrentTerrainTool == ETerrainTool::Raise)
		{
			ApplyHeightChange(TexelX, TexelY, true, DeltaTime);
		}
		else if (GameHUD->CurrentTerrainTool == ETerrainTool::Lower)
		{
			ApplyHeightChange(TexelX, TexelY, false, DeltaTime);
		}
	}
	//Painting
	if (bIsPaintingMode)
	{
		if (GameHUD->CurrentTerrainTool == ETerrainTool::Paint)
		{
			ApplyWeightChange(TexelX, TexelY, true, DeltaTime);
		}
		else if (GameHUD->CurrentTerrainTool == ETerrainTool::Erase)
		{
			ApplyWeightChange(TexelX, TexelY, false, DeltaTime);
		}
	}
}

void ATerrainMesh::HandleModeChange(EToolMode NewMode)
{
	if (!PlayerController) return;
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (!Subsystem) return;
	
	if (NewMode != EToolMode::Terrain)
	{
		bIsSculptingMode = false;
		bIsPaintingMode = false;
		BrushDecal->SetVisibility(false);
		Subsystem->RemoveMappingContext(IMC_Terraform);
		SetActorTickEnabled(false);
		return;
	}
	bIsSculptingMode = false;
	bIsPaintingMode = false;
	Subsystem->AddMappingContext(IMC_Terraform, 0);
	SetActorTickEnabled(true);
}

//Terrain Generation Functions
void ATerrainMesh::GenerateTerrain()
{
	//General index formula = index = y * W + x
	int32 W = GridSize + 1;

	for (int32 y = 0; y <= GridSize; y++)
	{
		for (int32 x = 0; x <= GridSize; x++)
		{
			Vertices.Add(FVector(x * QuadSize, y * QuadSize, 0.f));
			UVs.Add(FVector2D(x, y));
			VertexColors.Add(FColor(0,0,0,0));
		}
	}
	
	for (int32 y = 0; y < GridSize; y++)
	{
		for (int32 x = 0; x < GridSize; x++)
		{
			int32 i0 = y * W + x;
			int32 i1 = ((y + 1) * W) + x;
			int32 i2 = y*W + (x + 1);
			int32 i3 = (y + 1) * W + (x + 1);

			Triangles.Add(i0);
			Triangles.Add(i1);
			Triangles.Add(i2);

			Triangles.Add(i2);
			Triangles.Add(i1);
			Triangles.Add(i3);
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
}
void ATerrainMesh::RecalculateNormalsInRegion(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY)
{
	MinX = FMath::Max(MinX - 1, 0);
	MinY = FMath::Max(MinY - 1, 0);     
	MaxX = FMath::Min(MaxX + 1, GridSize);
	MaxY = FMath::Min(MaxY + 1, GridSize); 
	for (int y = MinY; y <= MaxY; y++)
	{
		for (int x = MinX; x <= MaxX; x++)
		{
			int32 W = GridSize + 1;
			int32 xl = FMath::Max(x - 1, 0),        xr = FMath::Min(x + 1, GridSize);
			int32 yd = FMath::Max(y - 1, 0),        yu = FMath::Min(y + 1, GridSize);

			float hL = Vertices[y  * W + xl].Z;
			float hR = Vertices[y  * W + xr].Z;
			float hD = Vertices[yd * W + x ].Z;
			float hU = Vertices[yu * W + x ].Z;

			FVector N(hL - hR, hD - hU, 2.0f * QuadSize);
			N.Normalize();
			Normals[y * W + x] = N;

			FVector T(2.0f * QuadSize, 0.0f, hR - hL);
			T.Normalize();
			Tangents[y * W + x] = FProcMeshTangent(T, false);/*flip it if tangents look weird*/
		}
	}
}

void ATerrainMesh::UpdateMesh()
{
	MeshComponent->UpdateMeshSection(0, Vertices, Normals, UVs, VertexColors, Tangents);
}

// Terraformation tools
void ATerrainMesh::WorldToTexel(const FVector& WorldPos, int32& VertexX, int32& VertexY) const
{
	FVector LocalPosition = GetActorTransform().InverseTransformPosition(WorldPos);
	
	VertexX = FMath::Clamp(FMath::RoundToInt32(LocalPosition.X / QuadSize), 0, GridSize);
	VertexY = FMath::Clamp(FMath::RoundToInt32(LocalPosition.Y / QuadSize), 0, GridSize);
	
}
bool ATerrainMesh::UpdateBrushPosition()
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
		BrushCenter = Hit.ImpactPoint;
		BrushNormal = Hit.ImpactNormal;
	}
	return bHit;
}

void ATerrainMesh::CalculateBrushBounds(int32 TexelX, int32 TexelY,
		int32& MinX, int32& MinY, int32& MaxX, int32& MaxY)
{
	MinX = 1;
	MinY = 1;
	MaxX = 0;
	MaxY = 0;
	
	if (!GameHUD) return;
	
	MinX = FMath::Clamp(TexelX - GameHUD->BrushSize , 0 , GridSize);
	MinY = FMath::Clamp(TexelY - GameHUD->BrushSize , 0 , GridSize);

	MaxX = FMath::Clamp(TexelX + GameHUD->BrushSize , 0, GridSize);
	MaxY = FMath::Clamp(TexelY + GameHUD->BrushSize , 0, GridSize);
}
//Sculpting
void ATerrainMesh::SculptStarted(){bIsSculptingMode = true;}
void ATerrainMesh::SculptFinished(){bIsSculptingMode = false;}

void ATerrainMesh::ApplyHeightChange(int32 TexelX, int32 TexelY, bool bIsRaising, float DeltaTime)
{
    if (!GameHUD) return;

    const float BrushSize = GameHUD->BrushSize;
    if (BrushSize <= 0.0f) return;
  
    int32 MinX, MinY, MaxX, MaxY;
    CalculateBrushBounds(TexelX, TexelY, MinX, MinY, MaxX, MaxY);

    const int32 Width = MaxX - MinX + 1;
    const int32 Height = MaxY - MinY + 1;
	
    for (int32 y = MinY; y <= MaxY; y++)
    {
        for (int32 x = MinX; x <= MaxX; x++)
        {
            const float dx = x - TexelX;
            const float dy = y - TexelY;
            const float distance = FMath::Sqrt(dx * dx + dy * dy);
            if (distance > BrushSize) continue;
        	
        	int32 W = GridSize + 1;
        	const int32 Index = y * W + x;

            const float DistNorm = distance / BrushSize;
            const float Falloff = 0.5f * (1.0f + FMath::Cos(DistNorm * PI));

            float DeltaHeight = SculptStrength * Falloff * DeltaTime;
            if (!bIsRaising) DeltaHeight = -DeltaHeight;

            float& Remainder = HeightRemainder.FindOrAdd(FIntPoint(x, y));
            Remainder += DeltaHeight;

            const float IntegerPart = FMath::TruncToFloat(Remainder);
            Remainder -= IntegerPart;

            if (IntegerPart != 0.0f)
            {
                const float NewHeight = static_cast<float>(Vertices[Index].Z) + IntegerPart;
                Vertices[Index].Z = FMath::RoundToFloat(NewHeight);
            }
        }
    }
	// Smoothing - Comparing height of a texel to it`s neighbours and 
    if (Width > 1 && Height > 1 && SmoothingStrength > 0.0f)
    {
        TArray<FVector> Smoothed = Vertices;

        for (int32 LocalY = 0; LocalY < Height; LocalY++)
        {
            for (int32 LocalX = 0; LocalX < Width; LocalX++)
            {
                float Sum = 0.0f;
                for (int32 oy = -1; oy <= 1; oy++)
                {
                    const int32 SampleY = FMath::Clamp(LocalY + oy, 0, Height - 1);
                    for (int32 ox = -1; ox <= 1; ox++)
                    {
                        const int32 SampleX = FMath::Clamp(LocalX + ox, 0, Width - 1);
                        Sum += static_cast<float>(Vertices[SampleY * Width + SampleX].Z);
                    }
                }
                const float Avg = Sum / 9.0f;
                const int32 Idx = LocalY * Width + LocalX;
                const float Blended = FMath::Lerp(static_cast<float>(Vertices[Idx].Z), Avg, SmoothingStrength);
                Smoothed[Idx].Z = static_cast<float>(FMath::RoundToFloat(Blended));
            }
        }
        Vertices = MoveTemp(Smoothed);
    }
	RecalculateNormalsInRegion(MinX, MinY, MaxX, MaxY);
	UpdateMesh();
}
//Painting
void ATerrainMesh::PaintStarted(){bIsPaintingMode = true;}
void ATerrainMesh::PaintFinished(){bIsPaintingMode = false;}

void ATerrainMesh::ApplyWeightChange(int32 TexelX, int32 TexelY, bool bIsPainting, float DeltaTime)
{
	if (!GameHUD) return;

	const float BrushSize = GameHUD->BrushSize;
	if (BrushSize <= 0.0f) return;
  
	int32 MinX, MinY, MaxX, MaxY;
	CalculateBrushBounds(TexelX, TexelY, MinX, MinY, MaxX, MaxY);

	const int32 Width = MaxX - MinX + 1;

	uint8 FColor::* Channel = nullptr;
	switch (GameHUD->CurrentPaintTexture)
	{
		case EPaintTexture::Dirt:     Channel = &FColor::R; break;
		case EPaintTexture::Pavement: Channel = &FColor::G; break;
		case EPaintTexture::Rocks:    Channel = &FColor::B; break;
		case EPaintTexture::Sand:     Channel = &FColor::A; break;
		case EPaintTexture::Grass:    Channel = nullptr;    break;
	}
	
	for (int32 y = MinY; y <= MaxY; y++)
	{
		for (int32 x = MinX; x <= MaxX; x++)
		{
			const float dx = x - TexelX;
			const float dy = y - TexelY;
			const float distance = FMath::Sqrt(dx * dx + dy * dy);
			if (distance > BrushSize) continue;

			int32 W = GridSize + 1;
			const int32 Index = y * W + x;

			const float DistNorm = distance / BrushSize;
			const float Falloff = 0.5f * (1.0f + FMath::Cos(DistNorm * PI));

			float DeltaWeight = PaintStrength * Falloff * DeltaTime;
			if (!bIsPainting) DeltaWeight = -DeltaWeight;

			float& Remainder = WeightRemainder.FindOrAdd(FIntPoint(x, y));
			Remainder += DeltaWeight;

			const float IntegerPart = FMath::TruncToFloat(Remainder);
			Remainder -= IntegerPart;

			if (IntegerPart != 0.0f)
			{
				// Swap to Render targets if more textures needed
				FColor& Color = VertexColors[Index];
				if (Channel && bIsPainting)
				{
					int32 Old       = Color.*Channel;
					int32 NewTarget = FMath::Clamp(Old + static_cast<int32>(IntegerPart), 0, 255);
					int32 Added     = NewTarget - Old;           
					Color.*Channel  = static_cast<uint8>(NewTarget);

					uint8 FColor::* All[4] = { &FColor::R, &FColor::G, &FColor::B, &FColor::A };

					int32 OthersSum{};
					for (uint8 FColor::* Ch : All)
						if (Ch != Channel) OthersSum += Color.*Ch;

					if (OthersSum > 0)
					{
						for (uint8 FColor::* Ch : All)
						{
							if (Ch == Channel) continue;
							int32 Take = FMath::RoundToInt(Added * (static_cast<float>(Color.*Ch) / OthersSum));
							Color.*Ch  = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Color.*Ch) - Take, 0, 255));
						}
					}
				}
				else
				{
					Color.R = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Color.R) - static_cast<int32>(FMath::Abs(IntegerPart)), 0, 255));
					Color.G = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Color.G) - static_cast<int32>(FMath::Abs(IntegerPart)), 0, 255));
					Color.B = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Color.B) - static_cast<int32>(FMath::Abs(IntegerPart)), 0, 255));
					Color.A = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Color.A) - static_cast<int32>(FMath::Abs(IntegerPart)), 0, 255));
				}
			}
		}
	}
	UpdateMesh();
}


