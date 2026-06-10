#pragma once

#include "BlockoutBaseDynamicMeshActor.h"

#include "BlockoutBaseGenerator.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutBaseGenerator : public ABlockoutBaseDynamicMeshActor
{
	GENERATED_BODY()

public:
	ABlockoutBaseGenerator();

	virtual void CreateBlockoutMesh() override;
	void GenerateBlockoutMesh();
	virtual void CPPGenerateBlockoutMesh();
	virtual void MeshOptimization();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="Smooth Normals", meta=(AllowPrivateAccess="true"))
	bool bSmoothNormal = false;
};
