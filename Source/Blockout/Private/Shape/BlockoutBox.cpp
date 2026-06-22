#include "Shape/BlockoutBox.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutBox::ABlockoutBox()
{
}

void ABlockoutBox::CPPGenerateBlockoutMesh()
{
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	FTransform Transform;
	Transform.SetLocation(FVector(BoxSize.X * 0.5f, BoxSize.Y * 0.5f, BoxSize.Z >= 0.0f ? 0.0f : BoxSize.Z));

	const int32 Steps = FMath::Max(Subdivision + 2, 0);

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		DynamicMeshComponent->GetDynamicMesh(),
		PrimitiveOptions,
		Transform,
		FMath::Abs(BoxSize.X),
		FMath::Abs(BoxSize.Y),
		FMath::Abs(BoxSize.Z),
		Steps,
		Steps,
		Steps,
		EGeometryScriptPrimitiveOriginMode::Base);
}
