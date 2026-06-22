#pragma once

#include "CoreMinimal.h"

#include "BlockoutEnum.generated.h"

UENUM(BlueprintType)
enum class EBlockoutMaterialPresetType : uint8
{
	Orange UMETA(DisplayName = "Orange"),
	Grey UMETA(DisplayName = "Grey"),
	Blue UMETA(DisplayName = "Blue"),
	Red UMETA(DisplayName = "Red"),
	Green UMETA(DisplayName = "Green"),
	Dark UMETA(DisplayName = "Dark"),
	White UMETA(DisplayName = "White"),
};

UENUM(BlueprintType)
enum class EBlockoutMaterialMode : uint8
{
	Custom UMETA(DisplayName = "Custom"),
	Blockout UMETA(DisplayName = "Blockout"),
};

UENUM(BlueprintType)
enum class EBlockoutSplinePointType : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Curve UMETA(DisplayName = "Curve"),
};

UENUM(BlueprintType)
enum class EBlockoutTextPlaceMode : uint8
{
	YZPositive UMETA(DisplayName = "+YZ"),
	XZPositive UMETA(DisplayName = "+XZ"),
	XYPositive UMETA(DisplayName = "+XY"),
	YZNegative UMETA(DisplayName = "-YZ"),
	XZNegative UMETA(DisplayName = "-XZ"),
	XYNegative UMETA(DisplayName = "-XY"),
};

UENUM(BlueprintType)
enum class EBlockoutIntervalMode : uint8
{
	Min UMETA(DisplayName = "Min"),
	Middle UMETA(DisplayName = "Middle"),
	Max UMETA(DisplayName = "Max"),
};

UENUM(BlueprintType)
enum class EBlockoutAlignment : uint8
{
	Left UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right UMETA(DisplayName = "Right"),
};

UENUM(BlueprintType)
enum class EBlockoutHorizontalAlignment : uint8
{
	Left UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right UMETA(DisplayName = "Right"),
};

UENUM(BlueprintType)
enum class EBlockoutVerticalAlignment : uint8
{
	Top UMETA(DisplayName = "Top"),
	Center UMETA(DisplayName = "Center"),
	Bottom UMETA(DisplayName = "Bottom"),
};

UENUM(BlueprintType)
enum class EBlockoutSphereTopoType : uint8
{
	Box UMETA(DisplayName = "Box"),
	LatLong UMETA(DisplayName = "LatLong"),
};

UENUM(BlueprintType)
enum class EBlockoutInstanceType : uint8
{
	BlockoutPreset UMETA(DisplayName = "Blockout Preset"),
	WorldBlockouts UMETA(DisplayName = "World Blockouts"),
};

UENUM(BlueprintType)
enum class EBlockoutStairStepType : uint8
{
	AdaptiveStepNumber UMETA(DisplayName = "Adaptive Step Number"),
	FixedStepNumber UMETA(DisplayName = "Fixed Step Number"),
};

UENUM(BlueprintType)
enum class EBlockoutRampControlType : uint8
{
	Height UMETA(DisplayName = "Height"),
	Angle UMETA(DisplayName = "Angle"),
};

UENUM(BlueprintType)
enum class EBlockoutPlaceMode : uint8
{
	Placement UMETA(DisplayName = "放置"),
	Box3D UMETA(DisplayName = "3D盒子"),
	Box2D UMETA(DisplayName = "2D盒子"),
	Line UMETA(DisplayName = "直线"),
};

UENUM(BlueprintType)
enum class EBlockoutInteractiveMode : uint8
{
	Box3D UMETA(DisplayName = "3D盒子"),
};

UENUM(BlueprintType)
enum class EBlockoutBoxAxis : uint8
{
	X UMETA(DisplayName = "X轴"),
	Y UMETA(DisplayName = "Y轴"),
	Z UMETA(DisplayName = "Z轴"),
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
