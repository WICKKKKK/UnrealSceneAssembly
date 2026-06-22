#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutCylinder.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Cylinder"))
class BLOCKOUT_API ABlockoutCylinder : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutCylinder();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="尺寸", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector Size = FVector(50.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0", UIMax="100"))
	int32 Subdivision = 20;
};
