#include "Shape/BlockoutTube.h"

#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutTube::ABlockoutTube()
{
}

void ABlockoutTube::CPPGenerateBlockoutMesh()
{
	const FVector NormalizedInner(InnerDiameter.X, InnerDiameter.Y, 0.0f);
	const FVector NormalizedOuter(OuterDiameter.X, OuterDiameter.Y, FMath::Max(OuterDiameter.Z, 0.01f));
	const float InnerRadius = FVector2D(NormalizedInner.X, NormalizedInner.Y).Length();
	const float OuterRadius = FVector2D(NormalizedOuter.X, NormalizedOuter.Y).Length();
	const float TubeWidth = FMath::Max(OuterRadius - InnerRadius, 0.01f);

	TArray<FVector2D> PolygonVertices;
	PolygonVertices.Reserve(4);
	PolygonVertices.Add(FVector2D::ZeroVector);
	PolygonVertices.Add(FVector2D(TubeWidth, 0.0f));
	PolygonVertices.Add(FVector2D(TubeWidth, NormalizedOuter.Z));
	PolygonVertices.Add(FVector2D(0.0f, NormalizedOuter.Z));

	FGeometryScriptRevolveOptions RevolveOptions;
	RevolveOptions.RevolveDegrees = TubeAngle;
	RevolveOptions.DegreeOffset = TubeStartAngle;
	RevolveOptions.bReverseDirection = false;
	RevolveOptions.bHardNormals = false;
	RevolveOptions.HardNormalAngle = 30.0f;
	RevolveOptions.bProfileAtMidpoint = false;
	RevolveOptions.bFillPartialRevolveEndcaps = true;

	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePolygon(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform::Identity,
		PolygonVertices,
		RevolveOptions,
		InnerRadius,
		FMath::Max(Subdivision + 2, 3));

	if (BevelDistance > 0.01f)
	{
		FGeometryScriptMeshBevelOptions BevelOptions;
		BevelOptions.BevelDistance = FMath::Min3<float>(BevelDistance, NormalizedOuter.Z * 0.5f, TubeWidth * 0.5f);
		BevelOptions.bInferMaterialID = false;
		BevelOptions.SetMaterialID = 0;
		BevelOptions.Subdivisions = 0;
		BevelOptions.RoundWeight = 1.0f;
		BevelOptions.bApplyFilterBox = false;
		BevelOptions.bFullyContained = true;

		UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(TargetMesh, BevelOptions);
	}
}
