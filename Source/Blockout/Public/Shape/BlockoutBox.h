#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutBox.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutBox : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutBox();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FBlockoutFVector BoxSize = FBlockoutFVector(100, 100, 100, false, "BoxSizeX", false, "BoxSizeY", false, "BoxSizeZ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", meta=(AllowPrivateAccess="true"))
	int32 Subdivision = 0;
};
