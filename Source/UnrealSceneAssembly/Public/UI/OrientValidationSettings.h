#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OrientValidationSettings.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EOrientValidationAxisOrder : uint8
{
	XYZ UMETA(DisplayName = "XYZ"),
	XZY UMETA(DisplayName = "XZY"),
	YXZ UMETA(DisplayName = "YXZ"),
	YZX UMETA(DisplayName = "YZX"),
	ZXY UMETA(DisplayName = "ZXY"),
	ZYX UMETA(DisplayName = "ZYX"),
};

UCLASS()
class UNREALSCENEASSEMBLY_API UOrientValidationSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "参数", meta = (DisplayName = "当前摆放资产", ToolTip = "用于朝向验证的 Static Mesh，可从 Content Browser 拖入。"))
	TObjectPtr<UStaticMesh> TargetMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Rotation计算", meta = (DisplayName = "计算Rotation", ToolTip = "开启：单图朝向输出世界 FRotator；关闭：使用旧的逐轴向量模式作为对照。"))
	bool bSingleImageComputeRotation = true;

	UPROPERTY(EditAnywhere, Category = "Rotation计算", meta = (DisplayName = "交换Front/Right", ToolTip = "在转换前交换 front/right 两列，用于将 XYZ+翻转列0 这类镜像基校正为合法 Rotation，同时对调 Front/Right 标签。"))
	bool bSingleImageSwapFrontRight = true;

	UPROPERTY(EditAnywhere, Category = "Rotation计算|M_basis", meta = (DisplayName = "轴顺序", ToolTip = "列向量映射顺序。6 种轴排列 x 3 个符号翻转 = 48 种 M_basis。"))
	EOrientValidationAxisOrder SingleImageAxisOrder = EOrientValidationAxisOrder::XYZ;

	UPROPERTY(EditAnywhere, Category = "Rotation计算|M_basis", meta = (DisplayName = "翻转列0", ToolTip = "对 M_basis 的第 0 列取负。"))
	bool bSingleImageFlipColumn0 = true;

	UPROPERTY(EditAnywhere, Category = "Rotation计算|M_basis", meta = (DisplayName = "翻转列1", ToolTip = "对 M_basis 的第 1 列取负。"))
	bool bSingleImageFlipColumn1 = false;

	UPROPERTY(EditAnywhere, Category = "Rotation计算|M_basis", meta = (DisplayName = "翻转列2 / Up", ToolTip = "对 M_basis 的第 2 列取负。"))
	bool bSingleImageFlipColumn2 = false;

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
