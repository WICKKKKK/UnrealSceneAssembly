#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutTube.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Tube"))
class BLOCKOUT_API ABlockoutTube : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutTube();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="内径", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector InnerDiameter = FVector(50.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="外径", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector OuterDiameter = FVector(100.0f, 0.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="圆管角度", meta=(AllowPrivateAccess="true", ClampMin="0.01", ClampMax="360.0", UIMin="0.01", UIMax="360.0"))
	float TubeAngle = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="圆管起始角度", meta=(AllowPrivateAccess="true", UIMin="0.0", UIMax="360.0"))
	float TubeStartAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="1", UIMin="1", UIMax="100"))
	int32 Subdivision = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="倒角距离", meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0"))
	float BevelDistance = 0.0f;
};
