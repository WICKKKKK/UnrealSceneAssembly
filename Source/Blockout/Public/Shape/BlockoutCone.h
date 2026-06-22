#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutCone.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Cone"))
class BLOCKOUT_API ABlockoutCone : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutCone();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="底部半径", meta=(AllowPrivateAccess="true", MakeEditWidget="true", ClampMin="0.01", UIMin="0.01"))
	FVector BaseRadius = FVector(50.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="顶部半径", meta=(AllowPrivateAccess="true", MakeEditWidget="true", ClampMin="0.01", UIMin="0.01"))
	FVector TopRadius = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="3", UIMin="3", UIMax="100"))
	int32 Subdivision = 20;
};
