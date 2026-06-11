#include "BlockoutInstancer.h"

#include "BlockoutLog.h"
#include "EngineUtils.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_EditorFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "LevelEditor.h"
#include "ISceneOutliner.h"
#include "Shape/BlockoutBox.h"

ABlockoutInstancer::ABlockoutInstancer()
{
	BlockoutPresetClass = ABlockoutBox::StaticClass();

	InstanceMeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("InstanceMeshComponent"));
	InstanceMeshComp->SetupAttachment(DynamicMeshComponent);
	InstanceMeshComp->SetVisibility(false);
	InstanceMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InstanceMeshComp->SetCastShadow(false);
}

void ABlockoutInstancer::CreateBlockoutMesh()
{
	if (!IsValid(InstanceMeshComp))
	{
		return;
	}

	GetInstanceMesh();
	PreprocessInstanceMesh();
	InstanceMeshPlacement();
}

void ABlockoutInstancer::SpawnPresetBlockoutActor()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (InstanceType == EBlockoutInstanceType::BlockoutPreset)
	{
		if (!IsValid(BlockoutPresetClass))
		{
			return;
		}

		const bool bReferenced = CheckPresetActorReferenced();
		if (IsValid(PresetBlockoutActor) && !bReferenced && PresetBlockoutActor->GetClass() != BlockoutPresetClass)
		{
			PresetBlockoutActor->Destroy();
			PresetBlockoutActor = nullptr;
		}
		else if (IsValid(PresetBlockoutActor) && bReferenced)
		{
			PresetBlockoutActor = Cast<ABlockoutBaseDynamicMeshActor>(UBlockoutLibrary_EditorFunctions::DuplicateActor(PresetBlockoutActor));
		}

		if (!IsValid(PresetBlockoutActor) || PresetBlockoutActor->GetClass() != BlockoutPresetClass)
		{
			bool bSpawnSuccess = false;
			PresetBlockoutActor = Cast<ABlockoutBaseDynamicMeshActor>(UBlockoutLibrary_EditorFunctions::SpawnActorInSublevel(BlockoutPresetClass, GetLevel(), FTransform::Identity, bSpawnSuccess));
		}

		if (!IsValid(PresetBlockoutActor))
		{
			return;
		}

		PresetBlockoutActor->UpdateCurrent(true, true, true);
		PresetBlockoutActor->bDisableSubtractiveComp = true;
		PresetBlockoutActor->bEnableSnapping = false;
		PresetBlockoutActor->bEnableCollisions = false;
		PresetBlockoutActor->bHiddenInGame = true;
		PresetBlockoutActor->Tags.AddUnique(TEXT("BlockoutPresetActor"));
		PresetBlockoutActor->SetActorLabel(FString::Printf(TEXT("__%s_Preset_%s__"), *GetName(), *BlockoutPresetClass->GetName()));
		PresetBlockoutActor->SetLockLocation(true);
	}
	else if (IsValid(PresetBlockoutActor))
	{
		PresetBlockoutActor->Destroy();
		PresetBlockoutActor = nullptr;
	}
}

void ABlockoutInstancer::SetPresetBlockoutActorProperties()
{
	if (!IsValid(PresetBlockoutActor))
	{
		return;
	}

	PresetBlockoutActor->SetActorTransform(GetActorTransform() * PresetTransform);
	if (PresetBlockoutActor->GetLevel() != GetLevel())
	{
		bool bMoveSuccess = false;
		UBlockoutLibrary_EditorFunctions::MoveActorsToSublevel({PresetBlockoutActor}, GetLevel(), bMoveSuccess);
	}

	PresetBlockoutActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	PresetBlockoutActor->SetActorHiddenInEditor(!bShowBlockoutPresetActor);
	PresetBlockoutActor->SetActorHiddenInOutliner(!bShowBlockoutPresetActor);

	const TWeakPtr<ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance();
	if (LevelEditor.IsValid())
	{
		const TSharedPtr<ISceneOutliner> SceneOutliner = LevelEditor.Pin()->GetMostRecentlyUsedSceneOutliner();
		if (SceneOutliner.IsValid())
		{
			SceneOutliner->FullRefresh();
		}
	}
}

bool ABlockoutInstancer::CheckPresetActorReferenced()
{
	if (!IsValid(PresetBlockoutActor))
	{
		return false;
	}

	for (TActorIterator<ABlockoutInstancer> It(GetWorld()); It; ++It)
	{
		ABlockoutInstancer* InstancerActor = *It;
		if (this != InstancerActor && PresetBlockoutActor == InstancerActor->GetPresetBlockoutActor())
		{
			return true;
		}
	}

	return false;
}

void ABlockoutInstancer::GetInstanceMesh()
{
	if (UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(InstanceMeshComp->GetDynamicMesh()))
	{
		InstanceMeshComp->GetDynamicMesh()->Reset();
	}

	if (InstanceType == EBlockoutInstanceType::BlockoutPreset)
	{
		BlockoutInstanceNum = 1;
		if (IsValid(PresetBlockoutActor) && IsValid(PresetBlockoutActor->GetGeneratedMeshComp()))
		{
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				InstanceMeshComp->GetDynamicMesh(),
				PresetBlockoutActor->GetGeneratedMeshComp()->GetDynamicMesh(),
				InstanceTransform.ToFTransform(),
				false,
				FGeometryScriptAppendMeshOptions());
		}
	}
	else
	{
		BlockoutInstanceNum = 0;
		for (TSoftObjectPtr<ABlockoutBaseDynamicMeshActor> BlockoutInstance : BlockoutActorList)
		{
			ABlockoutBaseDynamicMeshActor* Actor = BlockoutInstance.Get();
			if (IsValid(Actor) && Actor != this)
			{
				++BlockoutInstanceNum;
				if (BlockoutInstanceNum == 1)
				{
					BlockoutsPivot = Actor->GetActorLocation();
				}
				UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
					InstanceMeshComp->GetDynamicMesh(),
					Actor->GetDynamicMeshComponent()->GetDynamicMesh(),
					Actor->GetActorTransform() * InstanceTransform.ToFTransform(),
					false,
					FGeometryScriptAppendMeshOptions());
				Actor->SetActorHiddenInEditor(!bShowSelectedActors);
				Actor->bHiddenInGame = !bShowSelectedActors;
				Actor->bEnableCollisions = bShowSelectedActors;
			}
		}
	}
}

void ABlockoutInstancer::PreprocessInstanceMesh()
{
	InstanceMesh = AllocateComputeMesh();
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(InstanceMesh, InstanceMeshComp->GetDynamicMesh(), FTransform::Identity, false, FGeometryScriptAppendMeshOptions());

	if (bUnion && BlockoutInstanceNum > 1)
	{
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(InstanceMesh, FGeometryScriptMeshSelfUnionOptions());
	}

	if (bFilpNormal && !bSubtractive)
	{
		UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(InstanceMesh);
	}

	if (BlockoutInstanceNum > 0)
	{
		UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(InstanceMesh, BlockoutsPivot * -1.0f);
	}
}

void ABlockoutInstancer::CPPInstanceMeshPlacement()
{
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(DynamicMeshComponent->GetDynamicMesh(), InstanceMesh, FTransform::Identity, false, FGeometryScriptAppendMeshOptions());
}

void ABlockoutInstancer::InstanceMeshPlacement()
{
	CPPInstanceMeshPlacement();

	FEditorScriptExecutionGuard Guard;
	OnRebuildGeneratedMesh(DynamicMeshComponent->GetDynamicMesh());
}

void ABlockoutInstancer::UpdateCurrent(bool bForceRebuildBlockout, bool bForceRebuildInteractiveAffect, bool bRequestOverlappingBlockoutRebuild)
{
	Super::UpdateCurrent(bForceRebuildBlockout, bForceRebuildInteractiveAffect, bRequestOverlappingBlockoutRebuild);
	SetPresetBlockoutActorProperties();
}

void ABlockoutInstancer::UpdateAll()
{
	Super::UpdateAll();
	UBlockoutLibrary_EditorFunctions::DestroyNoReferencedPresetActors();
}

void ABlockoutInstancer::Destroyed()
{
	Super::Destroyed();
	if (IsValid(PresetBlockoutActor))
	{
		PresetBlockoutActor->Destroy();
	}
}

void ABlockoutInstancer::PostLoad()
{
	Super::PostLoad();
	GEditor->GetTimerManager()->SetTimerForNextTick([this]
	{
		SpawnPresetBlockoutActor();
		SetPresetBlockoutActorProperties();
	});
}

void ABlockoutInstancer::PostActorCreated()
{
	Super::PostActorCreated();
	SpawnPresetBlockoutActor();
	SetPresetBlockoutActorProperties();
}

void ABlockoutInstancer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SpawnPresetBlockoutActor();
	SetPresetBlockoutActorProperties();
}

void ABlockoutInstancer::PostEditImport()
{
	Super::PostEditImport();
	SpawnPresetBlockoutActor();
	SetPresetBlockoutActorProperties();
}
