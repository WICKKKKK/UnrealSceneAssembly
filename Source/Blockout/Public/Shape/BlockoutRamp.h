#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutRamp.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Ramp"))
class BLOCKOUT_API ABlockoutRamp : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutRamp();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="高度", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector Height = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="平面尺寸", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector PlanSize = FVector(100.0f, 100.0f, 0.0f);
};
