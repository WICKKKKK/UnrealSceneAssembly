#include "Building/BlockoutDoor.h"

#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutDoor::ABlockoutDoor()
{
}

void ABlockoutDoor::CPPGenerateBlockoutMesh()
{
	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	const float SafeDepth = FMath::Max(DepthX, 0.01f);
	const float SafeWidth = FMath::Max(WidthY, 0.01f);
	const float SafeHeight = FMath::Max(HeightZ, 0.01f);
	const float OuterCorner = FMath::Min3<float>(SafeWidth, SafeHeight, FMath::Max(OuterCornerRadius, 0.02f));

	FTransform OuterTransform;
	OuterTransform.SetLocation(FVector(SafeDepth * -1.0f, 0.0f, 0.0f));
	OuterTransform.SetRotation(FQuat(FRotator(-90.0f, 0.0f, 0.0f)));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		OuterTransform,
		FMath::Max((SafeHeight - OuterCorner) * 4.0f, 0.01f),
		FMath::Max((SafeWidth - OuterCorner) * 4.0f, 0.01f),
		OuterCorner,
		0,
		0,
		FMath::Max(OuterCornerSubdivision, 0));

	FGeometryScriptMeshExtrudeOptions OuterExtrudeOptions;
	OuterExtrudeOptions.ExtrudeDistance = SafeDepth * 2.0f;
	OuterExtrudeOptions.ExtrudeDirection = FVector(1.0f, 0.0f, 0.0f);
	OuterExtrudeOptions.UVScale = 1.0f;
	OuterExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TargetMesh, OuterExtrudeOptions);

	FGeometryScriptMeshPlaneCutOptions CutOptions;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(TargetMesh, FTransform(FQuat(FRotator(180.0f, 0.0f, 0.0f))), CutOptions);

	if (bGenerateBooleanRegion)
	{
		UDynamicMesh* CopyToMeshOut = SubtractiveMeshComp->GetDynamicMesh();
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(TargetMesh, SubtractiveMeshComp->GetDynamicMesh(), CopyToMeshOut);
	}
	else
	{
		SubtractiveMeshComp->GetDynamicMesh()->Reset();
	}

	const float InnerLimit = FMath::Min3<float>(SafeWidth, SafeHeight, FMath::Max(Thickness, 0.0f));
	if (Thickness >= FMath::Min(SafeWidth, SafeHeight))
	{
		return;
	}

	UDynamicMesh* InnerMesh = AllocateComputeMesh();
	const float InnerCorner = FMath::Min(FMath::Min(SafeWidth, SafeHeight), FMath::Max(InnerCornerRadius, 0.02f));

	FTransform InnerTransform;
	InnerTransform.SetLocation(FVector(SafeDepth * -1.1f, 0.0f, 0.0f));
	InnerTransform.SetRotation(FQuat(FRotator(-90.0f, 0.0f, 0.0f)));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
		InnerMesh,
		FGeometryScriptPrimitiveOptions(),
		InnerTransform,
		FMath::Max((SafeHeight - InnerLimit - InnerCorner) * 4.0f, 0.01f),
		FMath::Max((SafeWidth - InnerLimit - InnerCorner) * 4.0f, 0.01f),
		InnerCorner,
		0,
		0,
		FMath::Max(InnerCornerSubdivision, 0));

	FGeometryScriptMeshExtrudeOptions InnerExtrudeOptions;
	InnerExtrudeOptions.ExtrudeDistance = SafeDepth * 2.2f;
	InnerExtrudeOptions.ExtrudeDirection = FVector(1.0f, 0.0f, 0.0f);
	InnerExtrudeOptions.UVScale = 1.0f;
	InnerExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(InnerMesh, InnerExtrudeOptions);

	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		TargetMesh,
		FTransform::Identity,
		InnerMesh,
		FTransform::Identity,
		EGeometryScriptBooleanOperation::Subtract,
		FGeometryScriptMeshBooleanOptions());
}
