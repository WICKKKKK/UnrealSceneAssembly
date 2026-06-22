#pragma once

#include "BlockoutBaseGenerator.h"

#include "BlockoutSphere.generated.h"

UCLASS(NotBlueprintable, BlueprintType, meta=(BlockoutPlaceable, DisplayName="Sphere"))
class BLOCKOUT_API ABlockoutSphere : public ABlockoutBaseGenerator
{
	GENERATED_BODY()

public:
	ABlockoutSphere();
	virtual void CPPGenerateBlockoutMesh() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="球体拓扑类型", meta=(AllowPrivateAccess="true"))
	EBlockoutSphereTopoType SphereTopoType = EBlockoutSphereTopoType::Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="球体半径", meta=(AllowPrivateAccess="true", MakeEditWidget="true"))
	FVector SphereRadius = FVector(50.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Params", DisplayName="细分", meta=(AllowPrivateAccess="true", ClampMin="1", UIMin="1", UIMax="100"))
	int32 Subdivision = 10;
};
