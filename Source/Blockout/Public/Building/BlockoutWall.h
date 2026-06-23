#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutWall.generated.h"

class AActor;
class UBlockoutSplineComponent;

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Wall"))
class BLOCKOUT_API ABlockoutWall : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutWall();
	virtual void CPPGenerateBlockoutMesh() override;

protected:
	UPROPERTY(Category="DynamicMeshActor", BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	UBlockoutSplineComponent* SplineComp = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Spline", DisplayName="使用外部曲线", meta=(AllowPrivateAccess="true"))
	bool bUseOuterSpline = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Params|Spline", DisplayName="外部曲线", meta=(AllowPrivateAccess="true", EditCondition="bUseOuterSpline==true"))
	TObjectPtr<AActor> OuterSplineActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="墙厚度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float WallThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="墙高度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float WallHeight = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0", UIMax="100"))
	int32 Subdivision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="对齐方式", meta=(AllowPrivateAccess="true"))
	EBlockoutAlignment Alignment = EBlockoutAlignment::Left;
};
