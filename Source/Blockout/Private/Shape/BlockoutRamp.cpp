#include "Shape/BlockoutRamp.h"

#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

ABlockoutRamp::ABlockoutRamp()
{
}

void ABlockoutRamp::CPPGenerateBlockoutMesh()
{
	const FVector NormalizedPlan(PlanSize.X, PlanSize.Y, 0.0f);
	const FVector NormalizedHeight(0.0f, 0.0f, Height.Z);

	TArray<FVector2D> PolygonVertices;
	PolygonVertices.Reserve(4);
	PolygonVertices.Add(FVector2D::ZeroVector);
	PolygonVertices.Add(FVector2D(NormalizedHeight.Z, 0.0f));
	PolygonVertices.Add(FVector2D(NormalizedPlan.Z, NormalizedPlan.Y));
	PolygonVertices.Add(FVector2D(0.0f, NormalizedHeight.Y));

	FTransform PolygonTransform;
	PolygonTransform.SetRotation(FQuat(FRotator(90.0f, 0.0f, 0.0f)));
	PolygonTransform.SetLocation(FVector(NormalizedHeight.X, 0.0f, 0.0f));

	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTriangulatedPolygon(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		PolygonTransform,
		PolygonVertices,
		true);

	FGeometryScriptMeshExtrudeOptions ExtrudeOptions;
	ExtrudeOptions.ExtrudeDistance = NormalizedHeight.X + (NormalizedPlan.X * -1.0f);
	ExtrudeOptions.ExtrudeDirection = FVector(-1.0f, 0.0f, 0.0f);
	ExtrudeOptions.UVScale = 1.0f;
	ExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TargetMesh, ExtrudeOptions);
}
