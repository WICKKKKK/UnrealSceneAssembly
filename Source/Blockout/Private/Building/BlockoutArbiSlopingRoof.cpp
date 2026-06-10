#include "Building/BlockoutArbiSlopingRoof.h"

#include "BlockoutGeometryScriptCompat.h"
#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"


ABlockoutArbiSlopingRoof::ABlockoutArbiSlopingRoof()
{
	SetBlockoutMaterialPresetType(EBlockoutMaterialPresetType::Grey);

	SplineComp = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SplineComp->SetupAttachment(DynamicMeshComponent);
	FSplinePoint Point;
	SplineComp->AddPoint(Point, false);
	SplineComp->AddPoint(Point, false);
	SplineComp->AddPoint(Point, false);
	SplineComp->AddPoint(Point, false);
	TArray<FVector> PointsLocation;
	PointsLocation.Add(FVector(0.0f, 0.0f, 0.0f));
	PointsLocation.Add(FVector(600.0f, 0.0f, 0.0f));
	PointsLocation.Add(FVector(600.0f, 800.0f, 0.0f));
	PointsLocation.Add(FVector(0.0f, 800.0f, 0.0f));
	SplineComp->SetSplinePoints(PointsLocation, ESplineCoordinateSpace::Type::Local);

	SplineComp->SetClosedLoop(true, false);

	UBlockoutLibrary_SplineFunctions::SetSplinePointType(SplineComp, EBlockoutSplinePointType::Linear);
}

void ABlockoutArbiSlopingRoof::CPPGenerateBlockoutMesh()
{
	int PointNum = SplineComp->GetNumberOfSplinePoints();
	FVector SplineCenter = FVector::ZeroVector;
	for(int i=0; i<PointNum; ++i)
	{
		SplineCenter += SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Type::Local);
	}
	SplineCenter /= PointNum;

	RoofHeight.X = SplineCenter.X;
	RoofHeight.Y = SplineCenter.Y;
	RoofHeight.Z = FMath::Max(RoofHeight.Z, 0.01f);
	
	UBlockoutLibrary_SplineFunctions::ShowSplineID(SplineComp, this, bDisplaySplineId);
	
	UBlockoutLibrary_GeometryFunctions::GeneratePlanarMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		SplineComp,
		10);

	FVector RidgeDir;
	UBlockoutLibrary_GeometryFunctions::GenerateRoofMesh(
		DynamicMeshComponent->GetDynamicMesh(),
		GetComputeMeshPool(),
		RoofHeight.Z,
		RotateRidgeAngle,
		RidgeDir);
}
