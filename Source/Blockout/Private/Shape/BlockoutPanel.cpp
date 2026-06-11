#include "Shape/BlockoutPanel.h"

#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "UObject/ConstructorHelpers.h"

ABlockoutPanel::ABlockoutPanel()
{
	bApplyDefaultMaterial = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelMaterialFinder(
		TEXT("/UnrealSceneAssembly/BlockoutTools/Materials/MI_Grid_1m_Orange.MI_Grid_1m_Orange"));
	if (PanelMaterialFinder.Succeeded())
	{
		MaterialPanel = PanelMaterialFinder.Object;
		MaterialTopper = PanelMaterialFinder.Object;
	}
}

void ABlockoutPanel::CPPGenerateBlockoutMesh()
{
	FGeometryScriptPrimitiveOptions RoundRectOptions;
	FTransform PanelTransform;
	PanelTransform.SetLocation(FVector(PanelSize.X * 0.5f, PanelSize.Y * 0.5f, 0.0f));
	const float PanelCornerRadius = FMath::Max(FMath::Min(FMath::Min(PanelSize.X, PanelSize.Y) * 0.5f, CornerRadius), 0.01f);
	const float PanelDimensionX = PanelSize.X * 2.0f - PanelCornerRadius * 4.0f;
	const float PanelDimensionY = PanelSize.Y * 2.0f - PanelCornerRadius * 4.0f;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
		DynamicMeshComponent->GetDynamicMesh(),
		RoundRectOptions,
		PanelTransform,
		PanelDimensionX,
		PanelDimensionY,
		PanelCornerRadius,
		0,
		0,
		CornerSubdivision);

	FGeometryScriptMeshExtrudeOptions PanelExtrudeOptions;
	PanelExtrudeOptions.ExtrudeDistance = PanelSize.Z;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(DynamicMeshComponent->GetDynamicMesh(), PanelExtrudeOptions);

	if (bUseTopper)
	{
		UDynamicMesh* TopperMesh = AllocateComputeMesh();
		FTransform TopperTransform;
		TopperTransform.SetLocation(FVector(PanelSize.X * 0.5f, PanelSize.Y * 0.5f, PanelSize.Z + TopperOffset));

		const float ClampedTopperInsert = FMath::Min(FMath::Min(PanelSize.X, PanelSize.Y) * 0.5f, TopperInsert);
		const float TopperCornerRadius = FMath::Max(PanelCornerRadius - ClampedTopperInsert, 0.01f);
		const float TopperDimensionX = PanelSize.X * 2.0f - TopperCornerRadius * 4.0f - ClampedTopperInsert * 4.0f;
		const float TopperDimensionY = PanelSize.Y * 2.0f - TopperCornerRadius * 4.0f - ClampedTopperInsert * 4.0f;

		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
			TopperMesh,
			RoundRectOptions,
			TopperTransform,
			TopperDimensionX,
			TopperDimensionY,
			TopperCornerRadius,
			0,
			0,
			CornerSubdivision);

		FGeometryScriptMeshExtrudeOptions TopperExtrudeOptions;
		TopperExtrudeOptions.ExtrudeDistance = FMath::Max(TopperExtrude, 0.01f);
		UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TopperMesh, TopperExtrudeOptions);
		UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(TopperMesh);
		UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(TopperMesh, 0, 1);

		FGeometryScriptMeshBooleanOptions BooleanOptions;
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
			DynamicMeshComponent->GetDynamicMesh(),
			FTransform::Identity,
			TopperMesh,
			FTransform::Identity,
			BooleanOperator,
			BooleanOptions);
	}

	DynamicMeshComponent->ConfigureMaterialSet({MaterialPanel, MaterialTopper});
}
