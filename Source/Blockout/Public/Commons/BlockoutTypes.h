#pragma once

#include "CoreMinimal.h"

#include "BlockoutTypes.generated.h"

UENUM(BlueprintType)
enum class EBlockoutPlaceMode : uint8
{
	Placement UMETA(DisplayName = "Placement"),
	Box3D UMETA(DisplayName = "3D Box"),
	Box2D UMETA(DisplayName = "2D Box"),
	Line UMETA(DisplayName = "Line"),
};

UENUM(BlueprintType)
enum class EBlockoutInteractiveMode : uint8
{
	Box3D UMETA(DisplayName = "3D Box"),
};

UENUM(BlueprintType)
enum class EBlockoutBoxAxis : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z"),
};

USTRUCT(BlueprintType)
struct BLOCKOUT_API FBlockoutMaterialColor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FColor SurfaceColor = FColor::White;

	UPROPERTY(BlueprintReadWrite)
	FColor GridColor = FColor::Black;
};
