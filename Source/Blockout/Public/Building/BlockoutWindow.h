#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutWindow.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Window"))
class BLOCKOUT_API ABlockoutWindow : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutWindow();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="宽度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float Width = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="长度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float Length = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="高度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float Height = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Outer Corner", DisplayName="外拐角半径", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float OuterCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Outer Corner", DisplayName="外拐角细分", meta=(AllowPrivateAccess="true", ClampMin="2", UIMin="2", UIMax="50"))
	int32 OuterCornerSubdivision = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="生成窗户布尔区域", meta=(AllowPrivateAccess="true"))
	bool bGenerateWindowBooleanRegion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="厚度", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float Thickness = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="内拐角半径", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float InnerCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="内拐角细分", meta=(AllowPrivateAccess="true", ClampMin="2", UIMin="2", UIMax="50"))
	int32 InnerCornerSubdivision = 4;
};
