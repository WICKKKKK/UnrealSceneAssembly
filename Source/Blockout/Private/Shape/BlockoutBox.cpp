#include "Shape/BlockoutBox.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"

ABlockoutBox::ABlockoutBox()
{
}

void ABlockoutBox::CPPGenerateBlockoutMesh()
{
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	FTransform Transform;
	Transform.SetLocation(FVector(BoxSize.X.Value * 0.5f, BoxSize.Y.Value * 0.5f, 0.0f));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		DynamicMeshComponent->GetDynamicMesh(),
		PrimitiveOptions,
		Transform,
		BoxSize.X.Value,
		BoxSize.Y.Value,
		BoxSize.Z.Value);

	if (Subdivision > 0)
	{
		UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(
			DynamicMeshComponent->GetDynamicMesh(),
			FGeometryScriptPNTessellateOptions(),
			Subdivision);
	}
}
