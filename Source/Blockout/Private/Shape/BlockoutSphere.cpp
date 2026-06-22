#include "Shape/BlockoutSphere.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutSphere::ABlockoutSphere()
{
}

void ABlockoutSphere::CPPGenerateBlockoutMesh()
{
	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	const float Radius = FMath::Max(SphereRadius.Length(), 0.01f);
	const int32 StepsBase = FMath::Max(Subdivision + 2, 3);

	if (SphereTopoType == EBlockoutSphereTopoType::Box)
	{
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
			TargetMesh,
			FGeometryScriptPrimitiveOptions(),
			FTransform::Identity,
			Radius,
			StepsBase,
			StepsBase,
			StepsBase,
			EGeometryScriptPrimitiveOriginMode::Center);
	}
	else
	{
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(
			TargetMesh,
			FGeometryScriptPrimitiveOptions(),
			FTransform::Identity,
			Radius,
			StepsBase + 2,
			StepsBase + 4,
			EGeometryScriptPrimitiveOriginMode::Center);
	}
}
