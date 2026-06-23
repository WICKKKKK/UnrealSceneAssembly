#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutRoad.generated.h"

class AActor;
class UBlockoutSplineComponent;

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Road"))
class BLOCKOUT_API ABlockoutRoad : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutRoad();
	virtual void CPPGenerateBlockoutMesh() override;

protected:
	UPROPERTY(Category="DynamicMeshActor", BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	UBlockoutSplineComponent* SplineComp = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Spline", DisplayName="使用外部曲线", meta=(AllowPrivateAccess="true"))
	bool bUseOuterSpline = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Params|Spline", DisplayName="外部曲线", meta=(AllowPrivateAccess="true", EditCondition="bUseOuterSpline==true"))
	TObjectPtr<AActor> OuterSplineActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="道路宽度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float RoadWidth = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="道路厚度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float RoadThickness = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0", UIMax="100"))
	int32 Subdivision = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="对齐方式", meta=(AllowPrivateAccess="true"))
	EBlockoutAlignment Alignment = EBlockoutAlignment::Left;
};
