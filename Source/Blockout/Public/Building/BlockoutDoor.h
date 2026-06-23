#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutDoor.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Door"))
class BLOCKOUT_API ABlockoutDoor : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutDoor();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="深X", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float DepthX = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="宽Y", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float WidthY = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="高Z", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float HeightZ = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Outer Corner", DisplayName="外拐角半径", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float OuterCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Outer Corner", DisplayName="外拐角细分", meta=(AllowPrivateAccess="true", ClampMin="2", UIMin="2", UIMax="50"))
	int32 OuterCornerSubdivision = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="生成布尔区域", meta=(AllowPrivateAccess="true"))
	bool bGenerateBooleanRegion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="厚度", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float Thickness = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="内拐角半径", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float InnerCornerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Inner Corner", DisplayName="内拐角细分", meta=(AllowPrivateAccess="true", ClampMin="2", UIMin="2", UIMax="50"))
	int32 InnerCornerSubdivision = 4;
};
