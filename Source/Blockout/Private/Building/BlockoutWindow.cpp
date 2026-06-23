#include "Building/BlockoutWindow.h"

#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutWindow::ABlockoutWindow()
{
}

void ABlockoutWindow::CPPGenerateBlockoutMesh()
{
	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	const float SafeWidth = FMath::Max(Width, 0.01f);
	const float SafeLength = FMath::Max(Length, 0.01f);
	const float SafeHeight = FMath::Max(Height, 0.01f);
	const float HalfMinLengthHeight = FMath::Min(SafeLength, SafeHeight) * 0.5f;
	const float OuterCorner = FMath::Min(HalfMinLengthHeight, FMath::Max(OuterCornerRadius, 0.02f));

	FTransform OuterTransform;
	OuterTransform.SetLocation(FVector(SafeWidth * -0.5f, SafeLength * 0.5f, SafeHeight * 0.5f));
	OuterTransform.SetRotation(FQuat(FRotator(-90.0f, 0.0f, 0.0f)));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		OuterTransform,
		FMath::Max(SafeHeight * 2.0f - OuterCorner * 4.0f, 0.01f),
		FMath::Max(SafeLength * 2.0f - OuterCorner * 4.0f, 0.01f),
		OuterCorner,
		0,
		0,
		FMath::Max(OuterCornerSubdivision, 0));

	FGeometryScriptMeshExtrudeOptions OuterExtrudeOptions;
	OuterExtrudeOptions.ExtrudeDistance = SafeWidth;
	OuterExtrudeOptions.ExtrudeDirection = FVector(1.0f, 0.0f, 0.0f);
	OuterExtrudeOptions.UVScale = 1.0f;
	OuterExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TargetMesh, OuterExtrudeOptions);

	if (bGenerateWindowBooleanRegion)
	{
		UDynamicMesh* CopyToMeshOut = SubtractiveMeshComp->GetDynamicMesh();
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(TargetMesh, SubtractiveMeshComp->GetDynamicMesh(), CopyToMeshOut);
	}
	else
	{
		SubtractiveMeshComp->GetDynamicMesh()->Reset();
	}

	if (Thickness >= HalfMinLengthHeight)
	{
		return;
	}

	UDynamicMesh* InnerMesh = AllocateComputeMesh();
	const float InnerInset = FMath::Min(HalfMinLengthHeight, FMath::Max(Thickness, 0.0f));
	const float InnerCorner = FMath::Min(HalfMinLengthHeight, FMath::Max(InnerCornerRadius, 0.02f));
	const float InnerSubtractAmount = (InnerInset + InnerCorner) * 4.0f;

	FTransform InnerTransform;
	InnerTransform.SetLocation(FVector(SafeWidth * -0.55f, SafeLength * 0.5f, SafeHeight * 0.5f));
	InnerTransform.SetRotation(FQuat(FRotator(-90.0f, 0.0f, 0.0f)));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRoundRectangle_Compatibility_5_0(
		InnerMesh,
		FGeometryScriptPrimitiveOptions(),
		InnerTransform,
		FMath::Max(SafeHeight * 2.0f - InnerSubtractAmount, 0.01f),
		FMath::Max(SafeLength * 2.0f - InnerSubtractAmount, 0.01f),
		InnerCorner,
		0,
		0,
		FMath::Max(InnerCornerSubdivision, 0));

	FGeometryScriptMeshExtrudeOptions InnerExtrudeOptions;
	InnerExtrudeOptions.ExtrudeDistance = SafeWidth * 1.1f;
	InnerExtrudeOptions.ExtrudeDirection = FVector(1.0f, 0.0f, 0.0f);
	InnerExtrudeOptions.UVScale = 1.0f;
	InnerExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(InnerMesh, InnerExtrudeOptions);

	FGeometryScriptMeshBooleanOptions BooleanOptions;
	BooleanOptions.bFillHoles = false;
	BooleanOptions.SimplifyPlanarTolerance = 0.01f;
	BooleanOptions.bAllowEmptyResult = false;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		TargetMesh,
		FTransform::Identity,
		InnerMesh,
		FTransform::Identity,
		EGeometryScriptBooleanOperation::Subtract,
		BooleanOptions);
}
