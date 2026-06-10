#pragma once

#include "CoreMinimal.h"
#include "Solver/SceneAssemblyTypes.h"
#include "UObject/Object.h"
#include "SceneAssemblyTestSettings.generated.h"

UCLASS()
class UNREALSCENEASSEMBLY_API USceneAssemblyTestSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "检索", meta = (DisplayName = "候选数量", ToolTip = "每个 Actor 从 CLIP 检索返回的候选资产数量。", ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 CandidateLimit = 50;

	UPROPERTY(EditAnywhere, Category = "检索", meta = (DisplayName = "分数阈值", ToolTip = "CLIP 检索分数下限。0 表示不过滤。", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ScoreThreshold = 0.0f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "缩放模式", ToolTip = "FitIoU 使用三轴几何平均缩放；MatchHeight 使资产高度匹配白盒高度。"))
	ESceneAssemblyScaleMode ScaleMode = ESceneAssemblyScaleMode::FitIoU;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "组合模式", ToolTip = "Multiplicative 为非补偿式组合；Additive 为加权相加。"))
	ESceneAssemblyScoreCombineMode CombineMode = ESceneAssemblyScoreCombineMode::Multiplicative;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "语义权重", ToolTip = "语义分数权重。Multiplicative 模式下作为指数。", ClampMin = "0.0", UIMin = "0.0"))
	float WeightSemantic = 1.0f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "几何权重", ToolTip = "几何分数权重。Multiplicative 模式下作为指数。", ClampMin = "0.0", UIMin = "0.0"))
	float WeightGeometry = 1.0f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "缩放敏感度", ToolTip = "越大越惩罚整体缩放幅度。", ClampMin = "0.0", UIMin = "0.0"))
	float ScaleSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "比例敏感度", ToolTip = "越大越惩罚长宽高比例不一致。", ClampMin = "0.0", UIMin = "0.0"))
	float AspectSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "Top K", ToolTip = "保留的候选结果数量。1 表示选择最佳；大于 1 时在 Top-K 中按随机种子随机选取。", ClampMin = "1", UIMin = "1"))
	int32 TopK = 1;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "最终分数阈值", ToolTip = "低于该最终分数的候选会被过滤。", ClampMin = "0.0", UIMin = "0.0"))
	float FinalScoreThreshold = 0.0f;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "归一化语义分数", ToolTip = "在每次检索候选内部做 min-max 归一化。"))
	bool bNormalizeSemantic = false;

	UPROPERTY(EditAnywhere, Category = "求解器", meta = (DisplayName = "随机种子", ToolTip = "0 表示自动生成。Top K 为 1 时不会随机。", ClampMin = "0", UIMin = "0"))
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, Category = "摆放", meta = (DisplayName = "结果标签", ToolTip = "生成资产会带上该 Actor Tag；仅清理按钮也会删除该标签的生成结果。"))
	FString ResultTag = TEXT("SceneAssemblyResult");

	UPROPERTY(EditAnywhere, Category = "摆放", meta = (DisplayName = "仅处理白盒", ToolTip = "开启后只处理继承自 Blockout 基类的白盒 Actor，其他选中 Actor 会跳过。"))
	bool bWhiteboxOnly = true;
};
