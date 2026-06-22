#include "Shape/BlockoutCone.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutCone::ABlockoutCone()
{
}

void ABlockoutCone::CPPGenerateBlockoutMesh()
{
	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	const float BaseRadiusLength = FVector2D(BaseRadius.X, BaseRadius.Y).Length();
	const float TopRadiusLength = FVector2D(TopRadius.X, TopRadius.Y).Length();
	const float Height = FMath::Max(TopRadius.Z, 0.01f);
	const int32 RadialSteps = FMath::Max(Subdivision, 3);

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform::Identity,
		BaseRadiusLength,
		TopRadiusLength,
		Height,
		RadialSteps,
		4,
		true,
		EGeometryScriptPrimitiveOriginMode::Base);
}
