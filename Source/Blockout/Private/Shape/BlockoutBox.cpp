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
	Transform.SetLocation(FVector(BoxSize.X * 0.5f, BoxSize.Y * 0.5f, 0.0f));

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		DynamicMeshComponent->GetDynamicMesh(),
		PrimitiveOptions,
		Transform,
		BoxSize.X,
		BoxSize.Y,
		BoxSize.Z);

	if (Subdivision > 0)
	{
		UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(
			DynamicMeshComponent->GetDynamicMesh(),
			FGeometryScriptPNTessellateOptions(),
			Subdivision);
	}
}
