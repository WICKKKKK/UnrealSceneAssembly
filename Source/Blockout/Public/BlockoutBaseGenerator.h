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

	// 开启后则会将 40° 以内的法线进行平滑处理
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General", DisplayName="平滑法线", meta=(AllowPrivateAccess="true"))
	bool bSmoothNormal = false;
};
