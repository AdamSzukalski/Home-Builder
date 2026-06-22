#pragma once

#include "CoreMinimal.h"
#include "GameTypes.generated.h"

UENUM(BlueprintType)
enum class EToolMode : uint8
{
	Terrain UMETA(DisplayName = "Terrain"),
	Build UMETA(DisplayName = "Build"),
	Shop UMETA(DisplayName = "Shop"),
};

UENUM(BlueprintType)
enum class ETerrainTool : uint8
{
	Raise UMETA(DisplayName = "Raise"),
	Lower UMETA(DisplayName = "Lower"),
	Paint UMETA(DisplayName = "Paint"),
	Erase UMETA(DisplayName = "Erase"),
};

UENUM(BlueprintType)
enum class EBuildTool : uint8
{
	Wall UMETA(DisplayName = "Wall"),
	Door UMETA(DisplayName = "Door"),
	Window UMETA(DisplayName = "Window"),
	Roof UMETA(DisplayName = "Roof"),
	Delete UMETA(DisplayName = "Delete"),
};

UENUM(BlueprintType)
enum class EPaintTexture : uint8
{
	Grass UMETA(DisplayName = "Grass"),
	Dirt UMETA(DisplayName = "Dirt"),
	Pavement UMETA(DisplayName = "Pavement"),
	Rocks UMETA(DisplayName = "Rocks"),
	Sand UMETA(DisplayName = "Sand")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EToolMode, NewMode);