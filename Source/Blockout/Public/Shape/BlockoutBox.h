#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutBox.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Box"))
class BLOCKOUT_API ABlockoutBox : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutBox();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="尺寸", meta=(AllowPrivateAccess="true", MakeEditWidget="true", ClampMin="0.01", UIMin="0.01"))
	FVector BoxSize = FVector(100, 100, 100);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="0", UIMin="0", UIMax="100"))
	int32 Subdivision = 0;
};
