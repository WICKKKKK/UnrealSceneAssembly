#pragma once

#include "BlockoutBaseDynamicMeshActor.h"

#include "BlockoutInstancer.generated.h"

UCLASS(BlueprintType, Blueprintable)
class BLOCKOUT_API ABlockoutInstancer : public ABlockoutBaseDynamicMeshActor
{
	GENERATED_BODY()

public:
	ABlockoutInstancer();

	virtual void CreateBlockoutMesh() override;
	void SpawnPresetBlockoutActor();
	void SetPresetBlockoutActorProperties();
	bool CheckPresetActorReferenced();
	void GetInstanceMesh();
	void PreprocessInstanceMesh();
	virtual void InstanceMeshPlacement();
	virtual void CPPInstanceMeshPlacement();

	virtual void UpdateCurrent(bool bForceRebuildBlockout, bool bForceRebuildInteractiveAffect, bool bRequestOverlappingBlockoutRebuild) override;
	virtual void UpdateAll() override;
	virtual void Destroyed() override;
	virtual void PostActorCreated() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;

	ABlockoutBaseDynamicMeshActor* GetPresetBlockoutActor() const { return PresetBlockoutActor; }
	void SetPresetBlockoutActor(ABlockoutBaseDynamicMeshActor* InPresetBlockoutActor) { PresetBlockoutActor = InPresetBlockoutActor; }

protected:
	UPROPERTY(BlueprintReadOnly, Category="Instance")
	UDynamicMeshComponent* InstanceMeshComp = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Instance")
	UDynamicMesh* InstanceMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Instance")
	ABlockoutBaseDynamicMeshActor* PresetBlockoutActor = nullptr;

public:
	// 当前使用的实例模式, 可以选择使用预设白盒或者场景中的白盒 Actor 作为实例
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="实例模式", meta=(AllowPrivateAccess="true"))
	EBlockoutInstanceType InstanceType = EBlockoutInstanceType::BlockoutPreset;

	// 预设白盒类型, 可以选择所有已有的白盒类型作为实例
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="预设白盒类型", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset", EditConditionHides, BlueprintBaseOnly="true"))
	TSubclassOf<ABlockoutBaseDynamicMeshActor> BlockoutPresetClass;

	// 拾取场景中白盒 Actor 作为实例, 可以选择多个白盒 Actor, 结合 实例合并 功能可以将多个白盒布尔合并为一个 Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="场景白盒 Actor 列表", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::WorldBlockouts", EditConditionHides))
	TArray<TSoftObjectPtr<ABlockoutBaseDynamicMeshActor>> BlockoutActorList;

	// 显示拾取的白盒 Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="显示拾取的白盒 Actor", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::WorldBlockouts", EditConditionHides))
	bool bShowSelectedActors = true;

	int BlockoutInstanceNum = 0;
	FVector BlockoutsPivot = FVector::ZeroVector;

	// 如果当前拾取多个场景白盒 Actors, 可通过布尔合并将其合并为一个 Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="实例合并", meta=(AllowPrivateAccess="true"))
	bool bUnion = false;

	// 开启后翻转法线, 可用于室内Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="翻转法线", meta=(AllowPrivateAccess="true"))
	bool bFilpNormal = false;

	// 实例 Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="实例 Transform", meta=(AllowPrivateAccess="true"))
	FBlockoutTransform InstanceTransform;

	// 是否显示 Spawn 在场景中的预设白盒 Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="显示预设白盒 Actor", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset", EditConditionHides))
	bool bShowBlockoutPresetActor = false;

	// 预设白盒 Actor Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="预设白盒 Actor Transform", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset && bShowBlockoutPresetActor==true", EditConditionHides))
	FTransform PresetTransform = FTransform::Identity;
};
