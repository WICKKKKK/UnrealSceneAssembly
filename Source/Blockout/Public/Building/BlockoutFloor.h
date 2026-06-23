#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutFloor.generated.h"

class AActor;
class UBlockoutSplineComponent;

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Floor"))
class BLOCKOUT_API ABlockoutFloor : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutFloor();
	virtual void CPPGenerateBlockoutMesh() override;

protected:
	UPROPERTY(Category="DynamicMeshActor", BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	UBlockoutSplineComponent* SplineComp = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params|Spline", DisplayName="使用外部曲线", meta=(AllowPrivateAccess="true"))
	bool bUseOuterSpline = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Params|Spline", DisplayName="外部曲线", meta=(AllowPrivateAccess="true", EditCondition="bUseOuterSpline==true"))
	TObjectPtr<AActor> OuterSplineActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="楼板厚度", meta=(AllowPrivateAccess="true", ClampMin="0.01", UIMin="0.01"))
	float FloorThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0", UIMax="100"))
	int32 Subdivision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="对齐曲线方式", meta=(AllowPrivateAccess="true"))
	EBlockoutVerticalAlignment VerticalAlignment = EBlockoutVerticalAlignment::Top;
};
