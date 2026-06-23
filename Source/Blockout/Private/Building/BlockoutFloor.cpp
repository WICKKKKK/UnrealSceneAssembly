#include "Building/BlockoutFloor.h"

#include "BlockoutBuildingShapeUtils.h"
#include "Components/BlockoutSplineComponent.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"

ABlockoutFloor::ABlockoutFloor()
{
	SplineComp = CreateDefaultSubobject<UBlockoutSplineComponent>(TEXT("Spline"));
	SplineComp->SetupAttachment(DynamicMeshComponent);

	BlockoutBuildingShapeUtils::InitializeSpline(
		SplineComp,
		{FVector(0.0f, 0.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f), FVector(600.0f, 800.0f, 0.0f), FVector(0.0f, 800.0f, 0.0f)},
		true,
		EBlockoutSplinePointType::Linear);
}

void ABlockoutFloor::CPPGenerateBlockoutMesh()
{
	USplineComponent* SourceSpline = BlockoutBuildingShapeUtils::ResolveSpline(bUseOuterSpline ? OuterSplineActor.Get() : nullptr, SplineComp);
	if (!IsValid(SourceSpline) || SourceSpline->GetNumberOfSplinePoints() < 3)
	{
		return;
	}

	TArray<FVector> SplinePoints;
	FVector SplineCenter;
	BlockoutBuildingShapeUtils::GetSplinePointsWithInterp(SourceSpline, GetActorTransform(), SourceSpline != SplineComp, FMath::Max(Subdivision, 0), SplinePoints, SplineCenter);
	if (SplinePoints.Num() < 3)
	{
		return;
	}

	UDynamicMesh* FloorMesh = AllocateComputeMesh();
	BlockoutBuildingShapeUtils::AppendPlanarMeshFromPoints(FloorMesh, SplinePoints, VerticalAlignment);

	FGeometryScriptMeshExtrudeOptions ExtrudeOptions;
	ExtrudeOptions.ExtrudeDistance = FMath::Max(FloorThickness, 0.01f);
	ExtrudeOptions.ExtrudeDirection = FVector(0.0f, 0.0f, 1.0f);
	ExtrudeOptions.UVScale = 1.0f;
	ExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(FloorMesh, ExtrudeOptions);

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		FloorMesh,
		FTransform::Identity,
		false,
		FGeometryScriptAppendMeshOptions());
}
