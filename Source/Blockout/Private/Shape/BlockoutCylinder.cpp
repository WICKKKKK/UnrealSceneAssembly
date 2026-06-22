#include "Shape/BlockoutCylinder.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutCylinder::ABlockoutCylinder()
{
}

void ABlockoutCylinder::CPPGenerateBlockoutMesh()
{
	const FVector NormalizedSize(Size.X, Size.Y, FMath::Max(Size.Z, 0.01f));
	const float Radius = FVector2D(NormalizedSize.X, NormalizedSize.Y).Length();
	const int32 RadialSteps = FMath::Max(Subdivision + 2, 3);

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
		DynamicMeshComponent->GetDynamicMesh(),
		FGeometryScriptPrimitiveOptions(),
		FTransform::Identity,
		Radius,
		NormalizedSize.Z,
		RadialSteps,
		0,
		true,
		EGeometryScriptPrimitiveOriginMode::Base);
}
