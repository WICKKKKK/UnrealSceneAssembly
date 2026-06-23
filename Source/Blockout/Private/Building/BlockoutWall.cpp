#include "Building/BlockoutWall.h"

#include "BlockoutBuildingShapeUtils.h"
#include "Components/BlockoutSplineComponent.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"

ABlockoutWall::ABlockoutWall()
{
	SplineComp = CreateDefaultSubobject<UBlockoutSplineComponent>(TEXT("Spline"));
	SplineComp->SetupAttachment(DynamicMeshComponent);

	BlockoutBuildingShapeUtils::InitializeSpline(
		SplineComp,
		{FVector(-500.0f, 0.0f, 0.0f), FVector(500.0f, 0.0f, 0.0f)},
		false,
		EBlockoutSplinePointType::Linear);
}

void ABlockoutWall::CPPGenerateBlockoutMesh()
{
	USplineComponent* SourceSpline = BlockoutBuildingShapeUtils::ResolveSpline(bUseOuterSpline ? OuterSplineActor.Get() : nullptr, SplineComp);
	if (!IsValid(SourceSpline) || SourceSpline->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	TArray<FVector> SplinePoints;
	FVector SplineCenter;
	BlockoutBuildingShapeUtils::GetSplinePointsWithInterp(SourceSpline, GetActorTransform(), SourceSpline != SplineComp, FMath::Max(Subdivision, 0), SplinePoints, SplineCenter);
	if (SplinePoints.Num() < 2)
	{
		return;
	}

	UDynamicMesh* WallMesh = AllocateComputeMesh();
	BlockoutBuildingShapeUtils::AppendSplineRibbonFromPoints(WallMesh, SplinePoints, SourceSpline->IsClosedLoop(), WallThickness, Alignment);

	FGeometryScriptMeshExtrudeOptions ExtrudeOptions;
	ExtrudeOptions.ExtrudeDistance = FMath::Max(WallHeight, 0.01f);
	ExtrudeOptions.ExtrudeDirection = FVector(0.0f, 0.0f, 1.0f);
	ExtrudeOptions.UVScale = 1.0f;
	ExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(WallMesh, ExtrudeOptions);

	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		WallMesh,
		FTransform::Identity,
		false,
		FGeometryScriptAppendMeshOptions());
}
