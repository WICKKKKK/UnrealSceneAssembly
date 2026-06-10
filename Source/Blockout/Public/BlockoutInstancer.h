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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Instance Mode", meta=(AllowPrivateAccess="true"))
	EBlockoutInstanceType InstanceType = EBlockoutInstanceType::BlockoutPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Blockout Preset Class", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset", EditConditionHides, BlueprintBaseOnly="true"))
	TSubclassOf<ABlockoutBaseDynamicMeshActor> BlockoutPresetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="World Blockout Actor List", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::WorldBlockouts", EditConditionHides))
	TArray<TSoftObjectPtr<ABlockoutBaseDynamicMeshActor>> BlockoutActorList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Show Selected Actors", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::WorldBlockouts", EditConditionHides))
	bool bShowSelectedActors = true;

	int BlockoutInstanceNum = 0;
	FVector BlockoutsPivot = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Union Instances", meta=(AllowPrivateAccess="true"))
	bool bUnion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Flip Normals", meta=(AllowPrivateAccess="true"))
	bool bFilpNormal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", DisplayName="Instance Transform", meta=(AllowPrivateAccess="true"))
	FBlockoutTransform InstanceTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="Show Preset Actor", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset", EditConditionHides))
	bool bShowBlockoutPresetActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", DisplayName="Preset Actor Transform", meta=(AllowPrivateAccess="true", EditCondition="InstanceType==EBlockoutInstanceType::BlockoutPreset && bShowBlockoutPresetActor==true", EditConditionHides))
	FTransform PresetTransform = FTransform::Identity;
};
