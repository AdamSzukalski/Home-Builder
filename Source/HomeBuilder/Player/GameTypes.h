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
	None UMETA(DisplayName = "None"),
	Raise UMETA(DisplayName = "Raise"),
	Lower UMETA(DisplayName = "Lower"),
	Paint UMETA(DisplayName = "Paint"),
	Erase UMETA(DisplayName = "Erase"),
};

UENUM(BlueprintType)
enum class EBuildTool : uint8
{
	None UMETA(DisplayName = "None"),
	Floor UMETA(DisplayName = "Floor"),
	Wall UMETA(DisplayName = "Wall"),
	Door UMETA(DisplayName = "Door"),
	Window UMETA(DisplayName = "Window"),
	Roof UMETA(DisplayName = "Roof"),
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
UENUM(BlueprintType)
enum class ESelectionType : uint8
{
	None     UMETA(DisplayName="None"),
	Wall     UMETA(DisplayName="Wall"),
	Opening  UMETA(DisplayName="Opening"),
	Roof     UMETA(DisplayName="Roof")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EToolMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeleteRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCornerContext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHideCornerContext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeleteCornerRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUndoRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRedoRequested);