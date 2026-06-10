#pragma once

#include "BlockoutBaseGenerator.h"
#include "Components/SplineComponent.h"

#include "BlockoutArbiSlopingRoof.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutArbiSlopingRoof : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutArbiSlopingRoof();
	virtual void CPPGenerateBlockoutMesh() override;

protected:
	UPROPERTY(Category="DynamicMeshActor", BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	USplineComponent* SplineComp = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", MakeEditWidget="true", UIMin=0.0f, ClampMin=0.0f))
	FVector RoofHeight = FVector(0.0f, 0.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", UIMin=0.0f, UIMax=360.0f))
	float RotateRidgeAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true"))
	bool bDisplaySplineId = false;
};
