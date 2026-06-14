#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OrientValidationSettings.generated.h"

class UStaticMesh;

UCLASS()
class UNREALSCENEASSEMBLY_API UOrientValidationSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "参数", meta = (DisplayName = "当前摆放资产", ToolTip = "用于朝向验证的 Static Mesh，可从 Content Browser 拖入。"))
	TObjectPtr<UStaticMesh> TargetMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Dual Image 结果", meta = (DisplayName = "World Rotation"))
	FRotator DualImageWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category = "Dual Image 结果", meta = (DisplayName = "Relative Pose"))
	FString DualImageRelativePose;

	UPROPERTY(VisibleAnywhere, Category = "Dual Image 结果", meta = (DisplayName = "状态"))
	FString DualImageStatus = TEXT("尚未计算。");

	UPROPERTY(VisibleAnywhere, Category = "Precomputed 结果", meta = (DisplayName = "World Rotation"))
	FRotator PrecomputedWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, Category = "Precomputed 结果", meta = (DisplayName = "Relative Pose"))
	FString PrecomputedRelativePose;

	UPROPERTY(VisibleAnywhere, Category = "Precomputed 结果", meta = (DisplayName = "状态"))
	FString PrecomputedStatus = TEXT("尚未计算。");
};
