#include "Shape/BlockoutBox.h"

#include "BlockoutGeometryScriptCompat.h"

ABlockoutBox::ABlockoutBox()
{
}

void ABlockoutBox::CPPGenerateBlockoutMesh()
{
	FFalconGeometryScriptPrimitiveOptions PrimitiveOptions;
	FTransform Transform;
	Transform.SetLocation(FVector(BoxSize.X.Value * 0.5f, BoxSize.Y.Value * 0.5f, 0.0f));

	UFalconGeometryLibrary_MeshPrimitive::AppendBox(
		DynamicMeshComponent->GetDynamicMesh(),
		PrimitiveOptions,
		Transform,
		BoxSize.X.Value,
		BoxSize.Y.Value,
		BoxSize.Z.Value);

	if (Subdivision > 0)
	{
		UFalconGeometryLibrary_MeshSubdivide::ApplyPNTessellation(
			DynamicMeshComponent->GetDynamicMesh(),
			FFalconGeometryScriptPNTessellateOptions(),
			Subdivision);
	}
}
