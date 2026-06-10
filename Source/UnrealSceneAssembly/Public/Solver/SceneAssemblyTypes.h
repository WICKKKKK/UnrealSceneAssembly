#pragma once

#include "CoreMinimal.h"
#include "SceneAssemblyTypes.generated.h"

UENUM(BlueprintType)
enum class ESceneAssemblyScaleMode : uint8
{
	FitIoU UMETA(DisplayName = "Fit IoU"),
	MatchHeight UMETA(DisplayName = "Match Height"),
};

UENUM(BlueprintType)
enum class ESceneAssemblyScoreCombineMode : uint8
{
	Additive UMETA(DisplayName = "Additive"),
	Multiplicative UMETA(DisplayName = "Multiplicative"),
};

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FSceneOBB
{
	GENERATED_BODY()

	// OBB center in the local frame described by WorldTransform.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FVector LocalCenter = FVector::ZeroVector;

	// OBB half extents along local X/Y/Z. Local Z is treated as height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FVector HalfExtents = FVector::ZeroVector;

	// Local-to-world frame. Solver bakes any transform scale into LocalCenter/HalfExtents.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FTransform WorldTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FAssetCandidate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FString AssetPath;

	// Static mesh local bounds center, relative to asset pivot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FVector BboxCenter = FVector::ZeroVector;

	// Static mesh local bounds half extents.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FVector BboxHalfExtents = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float SemanticScore = 1.0f;
};

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FSolverSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	ESceneAssemblyScaleMode ScaleMode = ESceneAssemblyScaleMode::FitIoU;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	ESceneAssemblyScoreCombineMode CombineMode = ESceneAssemblyScoreCombineMode::Multiplicative;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver", meta = (ClampMin = "0.0"))
	float WeightSemantic = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver", meta = (ClampMin = "0.0"))
	float WeightGeometry = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver", meta = (ClampMin = "0.0"))
	float ScaleSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver", meta = (ClampMin = "0.0"))
	float AspectSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	bool bNormalizeSemantic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver", meta = (ClampMin = "1"))
	int32 TopK = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float FinalScoreThreshold = 0.0f;
};

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FPlacementResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FString AssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float FitIoU = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float ScaleFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float ScaleScore = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float SemanticScore = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	float FinalScore = 0.0f;

	// 0 = no horizontal axis swap, 1 = 90-degree local yaw swap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly|Solver")
	int32 YawStep = 0;
};
