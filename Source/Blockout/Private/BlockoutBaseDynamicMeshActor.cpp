#include "BlockoutBaseDynamicMeshActor.h"

#include "BlockoutLog.h"
#include "BlockoutSettings.h"
#include "Components/BillboardComponent.h"
#include "Components/BlockoutSplineComponent.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_EditorFunctions.h"
#include "Functions/BlockoutLibrary_FaceFunctions.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"
#include "Semantic/SceneSemanticComponent.h"
#include "UObject/ConstructorHelpers.h"

ABlockoutBaseDynamicMeshActor::ABlockoutBaseDynamicMeshActor()
{
	if (const UBlockoutSettings* Settings = UBlockoutSettings::Get())
	{
		ExportPath = Settings->DefaultExportPath;
	}
	else
	{
		ExportPath.Path = TEXT("/Game/Blockout/_Generated_Meshes");
	}

	SubtractiveMeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("SubtractiveMeshComponent"));
	SubtractiveMeshComp->SetupAttachment(DynamicMeshComponent);
	GeneratedMeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("GeneratedMeshComponent"));
	GeneratedMeshComp->SetupAttachment(DynamicMeshComponent);

	DynamicMeshComponent->SetComplexAsSimpleCollisionEnabled(true, false);
	SubtractiveMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SubtractiveMeshComp->SetCastShadow(false);
	GeneratedMeshComp->SetVisibility(false);
	GeneratedMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeneratedMeshComp->SetCastShadow(false);

#if WITH_EDITORONLY_DATA
	MainTextComp = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("TextLabel"));
	if (MainTextComp)
	{
		MainTextComp->SetMobility(EComponentMobility::Movable);
		MainTextComp->SetupAttachment(DynamicMeshComponent);
		MainTextComp->ComponentTags.AddUnique(TEXT("MainText"));
		MainTextComp->SetVisibility(false);
	}

	BoundingBoxComp = CreateEditorOnlyDefaultSubobject<UBlockoutBoxComponent>(TEXT("BoundingBox"));
	if (BoundingBoxComp)
	{
		BoundingBoxComp->SetMobility(EComponentMobility::Movable);
		BoundingBoxComp->SetupAttachment(DynamicMeshComponent);
		BoundingBoxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BoundingBoxComp->SetLineThicknessCustom(1.0f);
		BoundingBoxComp->SetVisibility(false);
	}
#endif

	SemanticComponent = CreateDefaultSubobject<USceneSemanticComponent>(TEXT("SceneSemantic"));

	Tags.AddUnique(TEXT("BlockoutActor"));
	PreviousLocation = GetActorLocation();
	PreviousRotation = GetActorRotation();
	InitializeTextPlacementLookupTable();

	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
			TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Grid_1m_Orange.MI_Grid_1m_Orange"));
		if (MaterialFinder.Succeeded())
		{
			CustomMaterial = MaterialFinder.Object;
		}
	}
}

FBox ABlockoutBaseDynamicMeshActor::CalMeshAABB(UDynamicMesh* InMesh, FTransform Transform)
{
	if (!UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(InMesh))
	{
		return FBox(FVector(-50.0f), FVector(50.0f));
	}

	UDynamicMesh* TransformedMesh = AllocateComputeMesh();
	UDynamicMesh* TransformedMeshOut = TransformedMesh;
	UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(InMesh, TransformedMesh, TransformedMeshOut);
	UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(TransformedMesh, Transform);
	const FBox AABB = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(TransformedMesh);
	ReleaseComputeMesh(TransformedMesh);
	return AABB;
}

void ABlockoutBaseDynamicMeshActor::CreateBlockoutMaterialInstance()
{
	UMaterialInterface* BlockoutMaterial = GetDefaultBlockoutMaterial();
	if (!IsValid(BlockoutMaterial))
	{
		return;
	}

	if (!IsValid(BlockoutMaterialInstance) || BlockoutMaterialInstance->Parent != BlockoutMaterial)
	{
		BlockoutMaterialInstance = UMaterialInstanceDynamic::Create(BlockoutMaterial, this);
	}
}

void ABlockoutBaseDynamicMeshActor::AssignCustomBlockoutMat(UDynamicMeshComponent* InDynamicMeshComp, FColor BlockoutColor, FColor GridColor, FBlockoutMaterialUVController InUVController)
{
	if (!IsValid(InDynamicMeshComp))
	{
		return;
	}

	if (bUseCustomMaterial)
	{
		InDynamicMeshComp->SetMaterial(0, IsValid(CustomMaterial) ? CustomMaterial : UMaterial::GetDefaultMaterial(MD_Surface));
		return;
	}

	CreateBlockoutMaterialInstance();
	if (!IsValid(BlockoutMaterialInstance))
	{
		return;
	}

	BlockoutMaterialInstance->SetVectorParameterValue(TEXT("Background_Color"), FLinearColor(BlockoutColor));
	BlockoutMaterialInstance->SetVectorParameterValue(TEXT("Grid_Color"), FLinearColor(GridColor));
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Scale_X"), InUVController.X.bReversed ? -1.0f : 1.0f);
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Offset_X"), InUVController.X.Offset);
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Scale_Y"), InUVController.Y.bReversed ? -1.0f : 1.0f);
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Offset_Y"), InUVController.Y.Offset);
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Scale_Z"), InUVController.Z.bReversed ? -1.0f : 1.0f);
	BlockoutMaterialInstance->SetScalarParameterValue(TEXT("Offset_Z"), InUVController.Z.Offset);

	InDynamicMeshComp->SetMaterial(0, BlockoutMaterialInstance);
}

void ABlockoutBaseDynamicMeshActor::AssignBlockoutMat()
{
	if (!DynamicMeshComponent || !DynamicMeshComponent->GetDynamicMesh())
	{
		return;
	}

	if (!bUseCustomMaterial && !bApplyDefaultMaterial)
	{
		return;
	}

	UBlockoutLibrary_BasicFunctions::ClearBlockoutMaterial(DynamicMeshComponent);
	const FBlockoutMaterialColor& MaterialColor = GetCurrentMaterialColor();
	AssignCustomBlockoutMat(DynamicMeshComponent, MaterialColor.SurfaceColor, MaterialColor.GridColor, UVController);
}

void ABlockoutBaseDynamicMeshActor::AssignSubtractiveMat()
{
	if (SubtractiveMeshComp && SubtractiveMeshComp->GetDynamicMesh() && GetSubtractiveMaterial())
	{
		SubtractiveMeshComp->SetMaterial(0, GetSubtractiveMaterial());
	}
}

bool ABlockoutBaseDynamicMeshActor::OverlappingDetection(ABlockoutBaseDynamicMeshActor* ActorA, bool bUseSubtractiveA, ABlockoutBaseDynamicMeshActor* ActorB, bool bUseSubtractiveB, float Tolerance)
{
	if (!IsValid(ActorA) || !IsValid(ActorB))
	{
		return false;
	}

	const FVector AMin = bUseSubtractiveA ? ActorA->GetSubtractiveMeshAABBMin() : ActorA->GetGeneratedMeshAABBMin();
	const FVector AMax = bUseSubtractiveA ? ActorA->GetSubtractiveMeshAABBMax() : ActorA->GetGeneratedMeshAABBMax();
	const FVector BMin = bUseSubtractiveB ? ActorB->GetSubtractiveMeshAABBMin() : ActorB->GetGeneratedMeshAABBMin();
	const FVector BMax = bUseSubtractiveB ? ActorB->GetSubtractiveMeshAABBMax() : ActorB->GetGeneratedMeshAABBMax();

	if ((AMax - AMin).GetAbsMin() <= Tolerance || (BMax - BMin).GetAbsMin() <= Tolerance)
	{
		return false;
	}

	return !(AMax.X <= BMin.X || BMax.X <= AMin.X || AMax.Y <= BMin.Y || BMax.Y <= AMin.Y || AMax.Z <= BMin.Z || BMax.Z <= AMin.Z);
}

bool ABlockoutBaseDynamicMeshActor::ValidateCurrentActor(ABlockoutBaseDynamicMeshActor* BlockoutActor, bool bUseSubtractive)
{
	if (!IsValid(BlockoutActor))
	{
		return false;
	}

	return bUseSubtractive
		? !BlockoutActor->bDisableSubtractiveComp
		: !BlockoutActor->bSubtractive && UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(BlockoutActor->GetDynamicMeshComponent()->GetDynamicMesh());
}

TArray<ABlockoutBaseDynamicMeshActor*> ABlockoutBaseDynamicMeshActor::GetOverlappingBlockoutActor(bool bUseSubtractiveTarget, bool bUseSubtractiveFound)
{
	TArray<ABlockoutBaseDynamicMeshActor*> FoundBlockoutActors;
	if (!ValidateCurrentActor(this, bUseSubtractiveTarget))
	{
		return FoundBlockoutActors;
	}

	for (TActorIterator<ABlockoutBaseDynamicMeshActor> It(GetWorld()); It; ++It)
	{
		ABlockoutBaseDynamicMeshActor* FoundBlockoutActor = *It;
		if (FoundBlockoutActor != this && ValidateCurrentActor(FoundBlockoutActor, bUseSubtractiveFound) && OverlappingDetection(this, bUseSubtractiveTarget, FoundBlockoutActor, bUseSubtractiveFound, 0.001f))
		{
			FoundBlockoutActors.Add(FoundBlockoutActor);
		}
	}
	return FoundBlockoutActors;
}

void ABlockoutBaseDynamicMeshActor::OverlappingBoolean()
{
	if (!bCanBeSubtracted || !UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(DynamicMeshComponent->GetDynamicMesh()))
	{
		return;
	}

	SubtractiveOverlappingBlockoutActors = GetOverlappingBlockoutActor(false, true);
	for (ABlockoutBaseDynamicMeshActor* FoundActor : SubtractiveOverlappingBlockoutActors)
	{
		UDynamicMesh* MeshToSubtract = AllocateComputeMesh();
		if (FoundActor->bUseBoundingBoxToSubtract)
		{
			const FTransform FoundActorTransform = FoundActor->GetTransform();
			const FBox FoundActorBBox = FoundActor->MeshLocalAABB;
			FVector BoxTranslation = FoundActorTransform.GetTranslation();
			BoxTranslation.X += (FoundActorBBox.Min.X + FoundActorBBox.Max.X) * 0.5f;
			BoxTranslation.Y += (FoundActorBBox.Min.Y + FoundActorBBox.Max.Y) * 0.5f;
			BoxTranslation.Z += FoundActorBBox.Min.Z;

			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
				MeshToSubtract,
				FGeometryScriptPrimitiveOptions(),
				FTransform(FoundActorTransform.GetRotation(), BoxTranslation, FoundActorTransform.GetScale3D()),
				FoundActorBBox.Max.X - FoundActorBBox.Min.X,
				FoundActorBBox.Max.Y - FoundActorBBox.Min.Y,
				FoundActorBBox.Max.Z - FoundActorBBox.Min.Z);
		}
		else
		{
			FTransform LocalToWorld;
			EGeometryScriptOutcomePins Outcome;
			UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(FoundActor->SubtractiveMeshComp, MeshToSubtract, FGeometryScriptCopyMeshFromComponentOptions(), true, LocalToWorld, Outcome);
			if (Outcome != EGeometryScriptOutcomePins::Success)
			{
				ReleaseComputeMesh(MeshToSubtract);
				continue;
			}
		}

		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
			DynamicMeshComponent->GetDynamicMesh(), GetActorTransform(), MeshToSubtract, FTransform::Identity,
			EGeometryScriptBooleanOperation::Subtract, FGeometryScriptMeshBooleanOptions());
		ReleaseComputeMesh(MeshToSubtract);
	}
}

void ABlockoutBaseDynamicMeshActor::SubtractiveRequestOverlappingBlockoutRebuild()
{
	if (!ValidateCurrentActor(this, true))
	{
		return;
	}

	for (TActorIterator<ABlockoutBaseDynamicMeshActor> It(GetWorld()); It; ++It)
	{
		ABlockoutBaseDynamicMeshActor* FoundBlockoutActor = *It;
		if (FoundBlockoutActor != this && ValidateCurrentActor(FoundBlockoutActor, false) && FoundBlockoutActor->bCanBeSubtracted &&
			(FoundBlockoutActor->SubtractiveOverlappingBlockoutActors.Contains(this) || OverlappingDetection(this, true, FoundBlockoutActor, false, 0.001f)))
		{
			FoundBlockoutActor->UpdateCurrent(false, true, false);
		}
	}
}

void ABlockoutBaseDynamicMeshActor::CreateBlockoutMesh()
{
	FEditorScriptExecutionGuard Guard;
	OnRebuildGeneratedMesh(DynamicMeshComponent->GetDynamicMesh());
}

void ABlockoutBaseDynamicMeshActor::SetBlockoutProperties()
{
	DynamicMeshComponent->SetVisibility(!bSubtractive);
	SubtractiveMeshComp->SetVisibility(bSubtractive);

	if (DynamicMeshComponent->GetVisibleFlag())
	{
		AssignBlockoutMat();
	}
	if (SubtractiveMeshComp->GetVisibleFlag())
	{
		AssignSubtractiveMat();
	}

	if (bUsePivotPreset)
	{
		OffsetPivot.X = PivotOffsetMode.X == EBlockoutIntervalMode::Min ? MeshLocalAABB.Min.X : (PivotOffsetMode.X == EBlockoutIntervalMode::Middle ? MeshLocalAABB.GetCenter().X : MeshLocalAABB.Max.X);
		OffsetPivot.Y = PivotOffsetMode.Y == EBlockoutIntervalMode::Min ? MeshLocalAABB.Min.Y : (PivotOffsetMode.Y == EBlockoutIntervalMode::Middle ? MeshLocalAABB.GetCenter().Y : MeshLocalAABB.Max.Y);
		OffsetPivot.Z = PivotOffsetMode.Z == EBlockoutIntervalMode::Min ? MeshLocalAABB.Min.Z : (PivotOffsetMode.Z == EBlockoutIntervalMode::Middle ? MeshLocalAABB.GetCenter().Z : MeshLocalAABB.Max.Z);
	}

	SetPivotOffset(OffsetPivot);
	SetActorEnableCollision(bEnableCollisions && !bSubtractive);
	SetActorHiddenInGame(bHiddenInGame || bSubtractive);
	DynamicMeshComponent->SetCastShadow(bCastShadows);
}

template <typename T>
static FString CreateTextLabelForProperty(FStructProperty* StructProperty, void* Container)
{
	if (StructProperty && StructProperty->Struct == T::StaticStruct())
	{
		if (T* PropertyValue = StructProperty->ContainerPtrToValuePtr<T>(Container))
		{
			return PropertyValue->ToString();
		}
	}
	return FString();
}

FString ABlockoutBaseDynamicMeshActor::CreateSingleUPropertyTextLabel(FProperty* Property)
{
	FString TextLabelString;
	FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (StructProperty)
	{
		TextLabelString = CreateTextLabelForProperty<FBlockoutFloat>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutInt>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutBool>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFString>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFVector>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFVector2D>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFVector4>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFIntVector>(StructProperty, this);
		if (TextLabelString.IsEmpty()) TextLabelString = CreateTextLabelForProperty<FBlockoutFIntVector2D>(StructProperty, this);
	}
	return TextLabelString;
}

void ABlockoutBaseDynamicMeshActor::InitializeTextPlacementLookupTable()
{
	TextPlacementLookupTable.Empty();
	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::YZPositive,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 180, 0), FVector(-0.501f, 0.0f, 0.0f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 180, 0), FVector(-0.501f, 0.0f, 0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 180, 0), FVector(-0.501f, 0.0f, -0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 180, 0), FVector(-0.501f, -0.5f, 0.0f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 180, 0), FVector(-0.501f, -0.5f, 0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 180, 0), FVector(-0.501f, -0.5f, -0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 180, 0), FVector(-0.501f, 0.5f, 0.0f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 180, 0), FVector(-0.501f, 0.5f, 0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 180, 0), FVector(-0.501f, 0.5f, -0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});

	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::YZNegative,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 0, 0), FVector(0.501f, 0.0f, 0.0f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 0, 0), FVector(0.501f, 0.0f, 0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 0, 0), FVector(0.501f, 0.0f, -0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 0, 0), FVector(0.501f, 0.5f, 0.0f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 0, 0), FVector(0.501f, 0.5f, 0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 0, 0), FVector(0.501f, 0.5f, -0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 0, 0), FVector(0.501f, -0.5f, 0.0f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 0, 0), FVector(0.501f, -0.5f, 0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 0, 0), FVector(0.501f, -0.5f, -0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});

	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::XZPositive,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, -90, 0), FVector(0.0f, -0.501f, 0.0f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, -90, 0), FVector(0.0f, -0.501f, 0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, -90, 0), FVector(0.0f, -0.501f, -0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, -90, 0), FVector(0.5f, -0.501f, 0.0f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, -90, 0), FVector(0.5f, -0.501f, 0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, -90, 0), FVector(0.5f, -0.501f, -0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, -90, 0), FVector(-0.5f, -0.501f, 0.0f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, -90, 0), FVector(-0.5f, -0.501f, 0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, -90, 0), FVector(-0.5f, -0.501f, -0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});

	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::XZNegative,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 90, 0), FVector(0.0f, 0.501f, 0.0f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 90, 0), FVector(0.0f, 0.501f, 0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 90, 0), FVector(0.0f, 0.501f, -0.5f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 90, 0), FVector(-0.5f, 0.501f, 0.0f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 90, 0), FVector(-0.5f, 0.501f, 0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 90, 0), FVector(-0.5f, 0.501f, -0.5f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(0, 90, 0), FVector(0.5f, 0.501f, 0.0f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(0, 90, 0), FVector(0.5f, 0.501f, 0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(0, 90, 0), FVector(0.5f, 0.501f, -0.5f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});

	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::XYPositive,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(-90, 0, 180), FVector(0.0f, 0.0f, -0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(-90, 0, 180), FVector(-0.5f, 0.0f, -0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(-90, 0, 180), FVector(0.5f, 0.0f, -0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(-90, 0, 180), FVector(0.0f, -0.5f, -0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(-90, 0, 180), FVector(-0.5f, -0.5f, -0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(-90, 0, 180), FVector(0.5f, -0.5f, -0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(-90, 0, 180), FVector(0.0f, 0.5f, -0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(-90, 0, 180), FVector(-0.5f, 0.5f, -0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(-90, 0, 180), FVector(0.5f, 0.5f, -0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});

	TextPlacementLookupTable.Add(EBlockoutTextPlaceMode::XYNegative,
	{
		{EBlockoutHorizontalAlignment::Center,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(90, 0, 180), FVector(0.0f, 0.0f, 0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(90, 0, 180), FVector(0.5f, 0.0f, 0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(90, 0, 180), FVector(-0.5f, 0.0f, 0.501f), EHorizTextAligment::EHTA_Center, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Left,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(90, 0, 180), FVector(0.0f, -0.5f, 0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(90, 0, 180), FVector(0.5f, -0.5f, 0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(90, 0, 180), FVector(-0.5f, -0.5f, 0.501f), EHorizTextAligment::EHTA_Left, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
		{EBlockoutHorizontalAlignment::Right,
			{
				{EBlockoutVerticalAlignment::Center, {FRotator(90, 0, 180), FVector(0.0f, 0.5f, 0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextCenter}},
				{EBlockoutVerticalAlignment::Top, {FRotator(90, 0, 180), FVector(0.5f, 0.5f, 0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextTop}},
				{EBlockoutVerticalAlignment::Bottom, {FRotator(90, 0, 180), FVector(-0.5f, 0.5f, 0.501f), EHorizTextAligment::EHTA_Right, EVerticalTextAligment::EVRTA_TextBottom}},
			}
		},
	});
}

void ABlockoutBaseDynamicMeshActor::PlaceTextLabelOnCubicFace(UTextRenderComponent* TextComp)
{
	if (!TextComp || !MeshLocalAABB.IsValid)
	{
		return;
	}

	if (const auto* HorizontalAlignments = TextPlacementLookupTable.Find(TextPlaceMode))
	{
		if (const auto* VerticalAlignments = HorizontalAlignments->Find(TextHorizontalAlignment))
		{
			if (const FTextPlacementParams* Params = VerticalAlignments->Find(TextVerticalAlignment))
			{
				const FVector Size = MeshLocalAABB.GetSize();
				const FVector Center = MeshLocalAABB.GetCenter();
				const FVector LocalOffset(Params->Offset.X * Size.X, Params->Offset.Y * Size.Y, Params->Offset.Z * Size.Z);
				TextComp->SetRelativeTransform(TextTransform.ToFTransform() * FTransform(Params->Rotation, Center + LocalOffset, FVector::OneVector));
				TextComp->SetHorizontalAlignment(Params->HorizontalAlignment);
				TextComp->SetVerticalAlignment(Params->VerticalAlignment);
				return;
			}
		}
	}

	BlockoutLog(TEXT("Text placement parameters not found for the given configuration."));
}

UTextRenderComponent* ABlockoutBaseDynamicMeshActor::CreateTextComp(FText InText, const FTransform& InTransform)
{
	UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(AddComponentByClass(UTextRenderComponent::StaticClass(), false, InTransform * TextTransform.ToFTransform(), false));
	if (TextComp)
	{
		TextComp->SetText(InText);
		TextComp->SetWorldSize(TextSize);
		TextComp->SetTextRenderColor(TextColor);
	}
	return TextComp;
}

void ABlockoutBaseDynamicMeshActor::SetAllTextCompProperty(bool bVisible, float InTextSize, FColor InTextColor)
{
	TArray<UTextRenderComponent*> TextLabelComps;
	GetComponents<UTextRenderComponent>(TextLabelComps, true);
	for (UTextRenderComponent* TextComp : TextLabelComps)
	{
		TextComp->SetVisibility(bVisible);
		TextComp->SetWorldSize(InTextSize);
		TextComp->SetTextRenderColor(InTextColor);
	}
}

void ABlockoutBaseDynamicMeshActor::CreatePropertyTextLabel()
{
	AllTextLabelString.Empty();
	for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
	{
		AllTextLabelString.Append(CreateSingleUPropertyTextLabel(*PropIt));
	}

	if (MainTextComp)
	{
		MainTextComp->SetText(FText::FromString(AllTextLabelString));
	}
}

void ABlockoutBaseDynamicMeshActor::GetBBox_Imp(FBox& OutLocalBox, FTransform& OutTransform)
{
	const bool CachedAllowEditorCall = GAllowActorScriptExecutionInEditor;
	GAllowActorScriptExecutionInEditor = true;
	GetBBox(OutLocalBox, OutTransform);
	if (OutLocalBox.GetSize().IsNearlyZero())
	{
		OutLocalBox.Min -= FVector(0.5f);
		OutLocalBox.Max += FVector(0.5f);
	}
	GAllowActorScriptExecutionInEditor = CachedAllowEditorCall;
}

void ABlockoutBaseDynamicMeshActor::RebuildBlockoutMesh()
{
	AllTextLabelString.Empty();

	if (UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(DynamicMeshComponent->GetDynamicMesh()))
	{
		DynamicMeshComponent->GetDynamicMesh()->Reset();
	}
	if (UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(SubtractiveMeshComp->GetDynamicMesh()))
	{
		SubtractiveMeshComp->GetDynamicMesh()->Reset();
	}

	CreateBlockoutMesh();

	GeneratedMeshComp->GetDynamicMesh()->Reset();
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(GeneratedMeshComp->GetDynamicMesh(), DynamicMeshComponent->GetDynamicMesh(), FTransform::Identity, false, FGeometryScriptAppendMeshOptions());
	if (bSubtractive)
	{
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(SubtractiveMeshComp->GetDynamicMesh(), DynamicMeshComponent->GetDynamicMesh(), FTransform::Identity, false, FGeometryScriptAppendMeshOptions());
	}

	if (bShowTextLabel && MainTextComp)
	{
		CreatePropertyTextLabel();
	}
}

void ABlockoutBaseDynamicMeshActor::BlockoutLog(FString InLog)
{
	UE_LOG(LogBlockout, Warning, TEXT("%s: %s"), *GetName(), *InLog);
}

void ABlockoutBaseDynamicMeshActor::RebuildInteractiveAffect()
{
	if (bCanBeSubtracted && !bSubtractive)
	{
		DynamicMeshComponent->GetDynamicMesh()->Reset();
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(DynamicMeshComponent->GetDynamicMesh(), GeneratedMeshComp->GetDynamicMesh(), FTransform::Identity, false, FGeometryScriptAppendMeshOptions());
		OverlappingBoolean();
	}
}

void ABlockoutBaseDynamicMeshActor::UpdateCurrent(bool bForceRebuildBlockout, bool bForceRebuildInteractiveAffect, bool bRequestOverlappingBlockoutRebuild)
{
	if (bFrozen || !IsValid(DynamicMeshComponent))
	{
		return;
	}

	bool bEnabledDeferredCollision = false;
	if (!DynamicMeshComponent->bDeferCollisionUpdates)
	{
		DynamicMeshComponent->SetDeferredCollisionUpdatesEnabled(true, false);
		bEnabledDeferredCollision = true;
	}

	if (bForceRebuildBlockout || bNeedRebuildBlockoutMesh)
	{
		RebuildBlockoutMesh();
		bNeedRebuildBlockoutMesh = false;
		bNeedRebuildInteractiveAffect = true;
	}

	GeneratedMeshAABB = CalMeshAABB(GeneratedMeshComp->GetDynamicMesh(), GetActorTransform());
	SubtractiveMeshAABB = CalMeshAABB(SubtractiveMeshComp->GetDynamicMesh(), GetActorTransform());

	if (bForceRebuildInteractiveAffect || bNeedRebuildInteractiveAffect)
	{
		RebuildInteractiveAffect();
		bNeedRebuildInteractiveAffect = false;
	}

	if (bRequestOverlappingBlockoutRebuild && bNeedRequestOverlappingBlockoutRebuild)
	{
		SubtractiveRequestOverlappingBlockoutRebuild();
		bNeedRequestOverlappingBlockoutRebuild = false;
	}

	if (bEnabledDeferredCollision)
	{
		DynamicMeshComponent->SetDeferredCollisionUpdatesEnabled(false, true);
	}

	ReleaseAllComputeMeshes();
	MeshLocalAABB = CalMeshAABB(bSubtractive ? SubtractiveMeshComp->GetDynamicMesh() : DynamicMeshComponent->GetDynamicMesh(), FTransform::Identity);

	if (bShowTextLabel && MainTextComp)
	{
		SetAllTextCompProperty(true, TextSize, TextColor);
		PlaceTextLabelOnCubicFace(MainTextComp);
	}
	else
	{
		SetAllTextCompProperty(false, TextSize, TextColor);
	}

	if (BoundingBoxComp)
	{
		BoundingBoxComp->SetVisibility(bShowBoundingBox);
		if (bShowBoundingBox)
		{
			if (BoundingBoxMode == EBlockoutBoundingBoxMode::LocalBox)
			{
				BoundingBoxComp->SetRelativeTransform(FTransform(MeshLocalAABB.GetCenter()));
				BoundingBoxComp->SetBoxExtent(MeshLocalAABB.GetExtent());
			}
			else if (BoundingBoxMode == EBlockoutBoundingBoxMode::GeneratedWorldBox)
			{
				BoundingBoxComp->SetWorldTransform(FTransform(GeneratedMeshAABB.GetCenter()));
				BoundingBoxComp->SetBoxExtent(GeneratedMeshAABB.GetExtent());
			}
			else
			{
				BoundingBoxComp->SetWorldTransform(FTransform(SubtractiveMeshAABB.GetCenter()));
				BoundingBoxComp->SetBoxExtent(SubtractiveMeshAABB.GetExtent());
			}
		}
	}

	SetBlockoutProperties();
}

void ABlockoutBaseDynamicMeshActor::RequestUpdateCurrent()
{
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildBlockoutMesh = true;
	bGeneratedMeshRebuildPending = true;
}

void ABlockoutBaseDynamicMeshActor::UpdateCurrentBlockout()
{
	UpdateCurrent(true, true, true);
}

void ABlockoutBaseDynamicMeshActor::UpdateAll()
{
	for (TActorIterator<ABlockoutBaseDynamicMeshActor> It(GetWorld()); It; ++It)
	{
		ABlockoutBaseDynamicMeshActor* BlockoutActor = *It;
		if (IsValid(BlockoutActor) && !BlockoutActor->Tags.Contains(TEXT("BlockoutPresetActor")))
		{
			BlockoutActor->UpdateCurrent(true, true, false);
		}
	}
}

void ABlockoutBaseDynamicMeshActor::ProfileAllBlockoutUpdate()
{
	UpdateAll();
}

void ABlockoutBaseDynamicMeshActor::ExportToStaticMeshActor(FString AssetExportPath)
{
	TArray<AActor*> OutActors;
	TArray<UObject*> OutAssets;
	ExportActorToLevel(GetLevel(), AssetExportPath, OutActors, OutAssets);
}

bool ABlockoutBaseDynamicMeshActor::ExportActorToLevel(ULevel* TargetLevel, FString AssetExportPath, TArray<AActor*>& OutActors, TArray<UObject*>& OutAssets)
{
	OutActors.Empty();
	OutAssets.Empty();
	if (bSubtractive || !IsValid(TargetLevel))
	{
		return false;
	}

	UpdateCurrent(true, true, false);
	if (!UEditorAssetLibrary::DoesDirectoryExist(AssetExportPath) && !UEditorAssetLibrary::MakeDirectory(AssetExportPath))
	{
		BlockoutLog(TEXT("Failed to create export directory: ") + AssetExportPath);
		return false;
	}

	FString UniqueAssetName;
	if (IsValid(ExportStaticMesh) && ExportStaticMesh->GetPathName() == AssetExportPath)
	{
		EGeometryScriptOutcomePins Outcome;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
			DynamicMeshComponent->GetDynamicMesh(),
			ExportStaticMesh,
			FGeometryScriptCopyMeshToAssetOptions(),
			FGeometryScriptMeshWriteLOD(),
			Outcome);
		if (Outcome == EGeometryScriptOutcomePins::Failure)
		{
			BlockoutLog(TEXT("CopyMeshToStaticMesh Failed"));
			return false;
		}

		UniqueAssetName = ExportStaticMesh->GetName();
	}
	else
	{
		FString UniqueAssetPathAndName;
		EGeometryScriptOutcomePins UniqueAssetNameOutcome;
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(AssetExportPath, TEXT("BlockoutMesh_"), UniqueAssetPathAndName, UniqueAssetName, FGeometryScriptUniqueAssetNameOptions(), UniqueAssetNameOutcome);
		if (UniqueAssetNameOutcome == EGeometryScriptOutcomePins::Failure)
		{
			return false;
		}

		EGeometryScriptOutcomePins CreateOutcome;
		FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
		CreateOptions.CollisionMode = DynamicMeshComponent->CollisionType;
		ExportStaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(DynamicMeshComponent->GetDynamicMesh(), UniqueAssetPathAndName, CreateOptions, CreateOutcome);
		if (CreateOutcome == EGeometryScriptOutcomePins::Failure || !IsValid(ExportStaticMesh))
		{
			return false;
		}
	}

	OutAssets.Add(ExportStaticMesh);
	UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromComponent(ExportStaticMesh, DynamicMeshComponent, FGeometryScriptSetSimpleCollisionOptions());
	if (UBodySetup* BodySetup = ExportStaticMesh->GetBodySetup())
	{
		BodySetup->CollisionTraceFlag = DynamicMeshComponent->CollisionType;
	}

	bool bSpawnSuccess = false;
	AStaticMeshActor* ExportedStaticMeshActor = Cast<AStaticMeshActor>(UBlockoutLibrary_EditorFunctions::SpawnActorInSublevel(AStaticMeshActor::StaticClass(), TargetLevel, GetActorTransform(), bSpawnSuccess));
	if (!bSpawnSuccess || !IsValid(ExportedStaticMeshActor))
	{
		return false;
	}

	ExportedActorTag = FString::Printf(TEXT("__%s_%s_ExportedActor__"), *GetName(), *UniqueAssetName);
	TArray<AActor*> ExportedActors = UBlockoutLibrary_EditorFunctions::GetOrSelectAllActorsByTag(*ExportedActorTag, false);
	for (AActor* ExportedActor : ExportedActors)
	{
		if (IsValid(ExportedActor))
		{
			ExportedActor->Destroy();
		}
	}

	ExportedStaticMeshActor->SetActorLabel(FString::Printf(TEXT("%s_Exported"), *GetName()));
	ExportedStaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(ExportStaticMesh);
	ExportedStaticMeshActor->Tags.AddUnique(TEXT("ExportedBlockoutActor"));
	ExportedStaticMeshActor->Tags.AddUnique(*ExportedActorTag);

	TArray<UMaterialInterface*> Materials = DynamicMeshComponent->GetMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = Materials[MaterialIndex];
		if (bExportMaterials)
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
			{
				Material = UBlockoutLibrary_BasicFunctions::ExportDynamicMaterialInstanceToMaterialInstance(MID, AssetExportPath, UniqueAssetName + TEXT("_Material_") + FString::FromInt(MaterialIndex));
				OutAssets.Add(Material);
			}
		}
		ExportedStaticMeshActor->GetStaticMeshComponent()->SetMaterial(MaterialIndex, Material);
		ExportStaticMesh->SetMaterial(MaterialIndex, Material);
	}

	if (AActor* ParentActor = GetAttachParentActor())
	{
		ExportedStaticMeshActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		ExportedStaticMeshActor->SetFolderPath(GetFolderPath());
	}

	OutActors.Add(ExportedStaticMeshActor);
	return true;
}

void ABlockoutBaseDynamicMeshActor::Export()
{
	ExportToStaticMeshActor(ExportPath.Path);
}

void ABlockoutBaseDynamicMeshActor::ShowBlockoutToolsPanel()
{
	UE_LOG(LogBlockout, Display, TEXT("BlockoutToolsPanel blueprint is not migrated in this module."));
}

void ABlockoutBaseDynamicMeshActor::UpdateBBox_Imp(const FBox& InLocalBox, const FTransform& InTransform, EBlockoutBoxAxis InMoveAxis)
{
	const bool CachedAllowEditorCall = GAllowActorScriptExecutionInEditor;
	GAllowActorScriptExecutionInEditor = true;
	UpdateBBox(InLocalBox, InTransform, InMoveAxis);
	GAllowActorScriptExecutionInEditor = CachedAllowEditorCall;
}

void ABlockoutBaseDynamicMeshActor::ExecuteRebuildGeneratedMeshIfPending()
{
	const FVector CurrentLocation = GetActorLocation() + GetActorRotation().RotateVector(GetPivotOffset());
	if (!CurrentLocation.Equals(PreviousLocation))
	{
		bGeneratedMeshRebuildPending = true;
		bNeedRebuildInteractiveAffect = true;
		bNeedRequestOverlappingBlockoutRebuild = true;
		PreviousLocation = CurrentLocation;
	}

	if (!bFrozen && bGeneratedMeshRebuildPending)
	{
		UpdateCurrent(false, false, true);
		bGeneratedMeshRebuildPending = false;
	}
}

void ABlockoutBaseDynamicMeshActor::SetActorHiddenInOutliner(bool bHiddenInOutliner)
{
	bListedInSceneOutliner = !bHiddenInOutliner;
}

void ABlockoutBaseDynamicMeshActor::SetActorHiddenInEditor(bool bHiddenInEditor)
{
	SetIsTemporarilyHiddenInEditor(bHiddenInEditor);
}

void ABlockoutBaseDynamicMeshActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bEnableSnapping && bFinished && bNeedUpdateSnapTransform)
	{
		SetActorTransform(SnapTransform);
		bNeedUpdateSnapTransform = false;
	}
}

void ABlockoutBaseDynamicMeshActor::PostActorCreated()
{
	Super::PostActorCreated();
}

void ABlockoutBaseDynamicMeshActor::PostEditImport()
{
	Super::PostEditImport();
}

void ABlockoutBaseDynamicMeshActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	// Heal legacy actors: instances saved before SemanticComponent became a default subobject may carry
	// an extra instance-created USceneSemanticComponent. Keep the default subobject and remove duplicates,
	// migrating any authored data onto the surviving component so the Details panel shows a single entry.
	TArray<USceneSemanticComponent*> SemanticComponents;
	GetComponents<USceneSemanticComponent>(SemanticComponents);
	if (SemanticComponents.Num() <= 1)
	{
		return;
	}

	USceneSemanticComponent* KeptComponent = SemanticComponent;
	if (!KeptComponent)
	{
		// Prefer the default subobject; otherwise keep the first non-instance component, else the first one.
		for (USceneSemanticComponent* Candidate : SemanticComponents)
		{
			if (Candidate && Candidate->CreationMethod != EComponentCreationMethod::Instance)
			{
				KeptComponent = Candidate;
				break;
			}
		}
		if (!KeptComponent)
		{
			KeptComponent = SemanticComponents[0];
		}
	}

	bool bModified = false;
	for (USceneSemanticComponent* Duplicate : SemanticComponents)
	{
		if (!Duplicate || Duplicate == KeptComponent)
		{
			continue;
		}

		// Migrate authored data from a stray duplicate if the kept component is still empty.
		if (KeptComponent)
		{
			const bool bKeptEmpty = KeptComponent->Category.IsEmpty() && KeptComponent->Description.IsEmpty() && KeptComponent->Tags.Num() == 0;
			const bool bDuplicateHasData = !Duplicate->Category.IsEmpty() || !Duplicate->Description.IsEmpty() || Duplicate->Tags.Num() > 0;
			if (bKeptEmpty && bDuplicateHasData)
			{
				KeptComponent->Modify();
				KeptComponent->Category = Duplicate->Category;
				KeptComponent->Description = Duplicate->Description;
				KeptComponent->Tags = Duplicate->Tags;
			}
		}

		Modify();
		RemoveInstanceComponent(Duplicate);
		Duplicate->DestroyComponent();
		bModified = true;
	}

	if (bModified)
	{
		SemanticComponent = KeptComponent;
		MarkPackageDirty();
	}
#endif
}

FBox ABlockoutBaseDynamicMeshActor::GetBoxFromBoundingBoxComp()
{
	if (!BoundingBoxComp)
	{
		return MeshLocalAABB;
	}
	const FVector Extent = BoundingBoxComp->GetScaledBoxExtent();
	const FVector Center = BoundingBoxComp->GetRelativeLocation();
	return FBox(Center - Extent, Center + Extent);
}

bool ABlockoutBaseDynamicMeshActor::FindNearestFace(FBlockoutFace& OutTargetFace, FBlockoutFace& OutOtherFace, float& OutNearestDistance, FVector& OutProjectionPoint)
{
	TArray<FBlockoutFace> AllOtherFaces;
	for (TActorIterator<ABlockoutBaseDynamicMeshActor> It(GetWorld()); It; ++It)
	{
		ABlockoutBaseDynamicMeshActor* OtherBlockout = *It;
		if (IsValid(OtherBlockout) && OtherBlockout != this && !OtherBlockout->Tags.Contains(TEXT("BlockoutPresetActor")))
		{
			AllOtherFaces.Append(UBlockoutLibrary_FaceFunctions::GetBoxFaces(OtherBlockout->GetBoxFromBoundingBoxComp(), OtherBlockout->GetActorTransform()));
		}
	}

	TArray<FBlockoutFace> TargetFaces = UBlockoutLibrary_FaceFunctions::GetBoxFaces(GetBoxFromBoundingBoxComp(), GetActorTransform());
	OutNearestDistance = SnapThreshold;
	bool bFoundFace = false;
	for (const FBlockoutFace& TargetFace : TargetFaces)
	{
		for (const FBlockoutFace& OtherFace : AllOtherFaces)
		{
			if (!UBlockoutLibrary_FaceFunctions::AreFacesParallel(TargetFace, OtherFace, AngleThreshold))
			{
				continue;
			}
			bool bInside = false;
			float ProjectDistance = 0.0f;
			const FVector ProjectionPoint = UBlockoutLibrary_FaceFunctions::ProjectPointToFace(TargetFace.Origin, OtherFace, bInside, ProjectDistance);
			if (bInside && ProjectDistance < OutNearestDistance)
			{
				OutNearestDistance = ProjectDistance;
				OutTargetFace = TargetFace;
				OutOtherFace = OtherFace;
				OutProjectionPoint = ProjectionPoint;
				bFoundFace = true;
			}
		}
	}
	return bFoundFace;
}

const FBlockoutMaterialColor& ABlockoutBaseDynamicMeshActor::GetMaterialColor(EBlockoutMaterialPresetType InPresetType)
{
	static TArray<FBlockoutMaterialColor> Colors = {
		{FColor(255, 153, 0, 0), FColor(0, 0, 0, 0)},
		{FColor(137, 137, 137, 0), FColor(0, 0, 0, 0)},
		{FColor(0, 173, 255, 0), FColor(0, 0, 0, 0)},
		{FColor(204, 0, 26, 0), FColor(0, 0, 0, 0)},
		{FColor(80, 176, 0, 0), FColor(0, 0, 0, 0)},
		{FColor(25, 25, 25, 0), FColor(95, 95, 95, 95)},
		{FColor(255, 255, 255, 0), FColor(0, 0, 0, 0)}
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(InPresetType), 0, Colors.Num() - 1);
	return Colors[Index];
}

const FBlockoutMaterialColor& ABlockoutBaseDynamicMeshActor::GetCurrentMaterialColor() const
{
	return GetMaterialColor(BlockoutMaterialPresetType);
}

UMaterialInterface* ABlockoutBaseDynamicMeshActor::GetDefaultBlockoutMaterial() const
{
	return UBlockoutSettings::Get()->GetBlockoutMaterial();
}

UMaterialInterface* ABlockoutBaseDynamicMeshActor::GetSubtractiveMaterial() const
{
	return UBlockoutSettings::Get()->GetSubtractiveMaterial();
}

void ABlockoutBaseDynamicMeshActor::BeginPlay()
{
	Super::BeginPlay();
	if (SubtractiveMeshComp)
	{
		SubtractiveMeshComp->SetVisibility(false);
	}
}

void ABlockoutBaseDynamicMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildBlockoutMesh = true;
}

void ABlockoutBaseDynamicMeshActor::PostEditUndo()
{
	Super::PostEditUndo();
	RegisterWithGenerationManager();
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildInteractiveAffect = true;
}

FName ABlockoutBaseDynamicMeshActor::GetCustomIconName() const
{
	return TEXT("BlockoutBaseDynamicMeshActor");
}

void ABlockoutBaseDynamicMeshActor::Destroyed()
{
	TArray<TWeakObjectPtr<ABlockoutBaseDynamicMeshActor>> WeakFoundActors;
	for (ABlockoutBaseDynamicMeshActor* Actor : GetOverlappingBlockoutActor(true, false))
	{
		WeakFoundActors.Add(Actor);
	}

	Super::Destroyed();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([WeakFoundActors]()
		{
			for (TWeakObjectPtr<ABlockoutBaseDynamicMeshActor> FoundActor : WeakFoundActors)
			{
				if (FoundActor.IsValid() && !FoundActor->bSubtractive && FoundActor->bCanBeSubtracted && UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(FoundActor->GetDynamicMeshComponent()->GetDynamicMesh()))
				{
					FoundActor->UpdateCurrent(false, true, false);
				}
			}
		});
	}
}

void ABlockoutBaseDynamicMeshActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildInteractiveAffect = true;
}

void ABlockoutBaseDynamicMeshActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildInteractiveAffect = true;
}

void ABlockoutBaseDynamicMeshActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyScale(DeltaScale, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
	bNeedRequestOverlappingBlockoutRebuild = true;
	bNeedRebuildInteractiveAffect = true;
}
