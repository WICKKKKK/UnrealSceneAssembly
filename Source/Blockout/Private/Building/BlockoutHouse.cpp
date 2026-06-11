#include "Building/BlockoutHouse.h"

#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
ABlockoutHouse::ABlockoutHouse()
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
	PointsLocation.Add(FVector(1000.0f, 0.0f, 0.0f));
	PointsLocation.Add(FVector(1000.0f, 1000.0f, 0.0f));
	PointsLocation.Add(FVector(0.0f, 1000.0f, 0.0f));
	SplineComp->SetSplinePoints(PointsLocation, ESplineCoordinateSpace::Type::Local);

	SplineComp->SetClosedLoop(true, false);

	UBlockoutLibrary_SplineFunctions::SetSplinePointType(SplineComp, EBlockoutSplinePointType::Linear);
	
}


void ABlockoutHouse::CPPGenerateBlockoutMesh()
{
	TArray<int> OutSplineIndexes;
	TArray<FVector> OutSplinePoints;
	TArray<FVector> OutTangetList;
	FVector OutSplineCenter;
	UBlockoutLibrary_SplineFunctions::GetSplinePointLocationWithInterp(SplineComp, 0, OutSplineIndexes,
		OutSplinePoints, OutTangetList, OutSplineCenter);

	BaseFloorThickness = FVector(OutSplineCenter.X, OutSplineCenter.Y, FMath::Max(BaseFloorThickness.Z, 0.01f));

	WallHeight = FVector(BaseFloorThickness.X, BaseFloorThickness.Y, FMath::Max(WallHeight.Z, BaseFloorThickness.Z+0.01f));

	int FloorNum = FMath::TruncToInt((WallHeight.Z - BaseFloorThickness.Z) / FloorHeight);

	UDynamicMesh* PlanMesh = AllocateComputeMesh();
	
	if (bGenerateFloor || bGenerateRoof)
	{
		UBlockoutLibrary_GeometryFunctions::GeneratePlanarMesh(
			PlanMesh,
			SplineComp,
			10
			);
	}

	if (bGenerateFloor)
	{
		UDynamicMesh* BaseFloorMesh = AllocateComputeMesh();
		
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(
			PlanMesh,
			BaseFloorMesh,
			BaseFloorMesh
			);

		FGeometryScriptMeshExtrudeOptions FloorExtrudeOptions;
		FloorExtrudeOptions.ExtrudeDistance = BaseFloorThickness.Z;
		UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(
			BaseFloorMesh,
			FloorExtrudeOptions
			);

		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			DynamicMeshComponent->GetDynamicMesh(),
			BaseFloorMesh,
			FTransform::Identity,
			false,
			FGeometryScriptAppendMeshOptions()
			);

		if (bGenerateMiddleFloor)
		{
			if (FloorNum >= 2)
			{
				UDynamicMesh* MiddleFloorMesh = AllocateComputeMesh();
				UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(
					PlanMesh,
					MiddleFloorMesh,
					MiddleFloorMesh
					);

				FloorExtrudeOptions.ExtrudeDistance = MiddleFloorThickness;
				UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(
					MiddleFloorMesh,
					FloorExtrudeOptions
					);

				UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(
					MiddleFloorMesh,
					FVector(0.0f, 0.0f, BaseFloorThickness.Z)
					);

				UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
					DynamicMeshComponent->GetDynamicMesh(),
					MiddleFloorMesh,
					FTransform(FVector(0.0f, 0.0f, (WallHeight.Z-BaseFloorThickness.Z)/float(FloorNum))),
					FloorNum-1,
					true,
					false,
					FGeometryScriptAppendMeshOptions()
					);
			}
		}
	}

	if (bGenerateWall)
	{
		UDynamicMesh* WallMesh = AllocateComputeMesh();
		UBlockoutLibrary_GeometryFunctions::GenerateSplineMesh(
			WallMesh,
			SplineComp,
			WallThickness,
			0,
			true,
			false
			);

		FGeometryScriptMeshExtrudeOptions WallExtrudeOptions;
		WallExtrudeOptions.ExtrudeDistance = WallHeight.Z - BaseFloorThickness.Z;
		UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(
			WallMesh,
			WallExtrudeOptions
			);

		UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(
			WallMesh,
			FVector(0.0f, 0.0f, BaseFloorThickness.Z)
			);

		if (bGenerateHoles)
		{
			HoleSize.Y = HoleSize.X;
			
			UDynamicMesh* AllWindowMesh = AllocateComputeMesh();
			UDynamicMesh* WindowMesh = AllocateComputeMesh();

			FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
				WindowMesh,
				PrimitiveOptions,
				FTransform::Identity,
				HoleSize.X,
				HoleSize.Y,
				HoleSize.Z
				);

			int WindowNum = FMath::TruncToInt(SplineComp->GetSplineLength() / HoleInterval);
			float IOfCurWindow = 1.0f/float(WindowNum);
			FTransform AppendTransform = FTransform::Identity;
			
			for (int i=0; i<WindowNum; ++i)
			{
				FTransform CurLocTransform = SplineComp->GetTransformAtTime(
					FMath::Clamp(float(i)*IOfCurWindow, 0.0f, 1.0f), ESplineCoordinateSpace::Local, true);
				AppendTransform.SetLocation(CurLocTransform.GetLocation() + FVector(0.0f, 0.0f, BaseFloorThickness.Z));
				AppendTransform.SetRotation(FQuat(FRotator(0.0f, FRotator(CurLocTransform.GetRotation()).Yaw,0.0f)));
				UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
					AllWindowMesh,
					WindowMesh,
					AppendTransform,
					false,
					FGeometryScriptAppendMeshOptions()
					);
			}

			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMeshRepeated(
				AllWindowMesh,
				AllWindowMesh,
				FTransform(FVector(0.0f, 0.0f, (WallHeight.Z-BaseFloorThickness.Z)/float(FloorNum))),
				FloorNum-1,
				true,
				false,
				FGeometryScriptAppendMeshOptions()
				);
			UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
				WallMesh,
				FTransform::Identity,
				AllWindowMesh,
				FTransform(FVector(0.0f, 0.0f, HoleHeightOffset)),
				EGeometryScriptBooleanOperation::Subtract,
				FGeometryScriptMeshBooleanOptions()
				);
		}
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			DynamicMeshComponent->GetDynamicMesh(),
			WallMesh,
			FTransform::Identity,
			false,
			FGeometryScriptAppendMeshOptions()
			);
	}

	if (bGenerateRoof)
	{
		UDynamicMesh* RoofMesh = AllocateComputeMesh();
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(
			PlanMesh,
			RoofMesh,
			RoofMesh
			);

		FVector RidgeDir;
		FGeometryScriptMeshExtrudeOptions FlatRoofExtrudeOptions;
		
		switch(RoofType)
		{
		case EHouseRoofType::SlopingRoof:
			UBlockoutLibrary_GeometryFunctions::GenerateRoofMesh(
				RoofMesh,
				GetComputeMeshPool(),
				SlopingRoofHeight,
				RotateRidgeAngle,
				RidgeDir
				);

			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				DynamicMeshComponent->GetDynamicMesh(),
				RoofMesh,
				FTransform(FVector(0.0f, 0.0f, WallHeight.Z)),
				false,
				FGeometryScriptAppendMeshOptions()
				);
			break;
		case EHouseRoofType::FlatRoof:
			FlatRoofExtrudeOptions.ExtrudeDistance = RoofThickness;
			UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(
				RoofMesh,
				FlatRoofExtrudeOptions
				);
			
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				DynamicMeshComponent->GetDynamicMesh(),
				RoofMesh,
				FTransform(FVector(0.0f, 0.0f, WallHeight.Z)),
				false,
				FGeometryScriptAppendMeshOptions()
				);

			if (bWithParapet)
			{
				UDynamicMesh* ParapetMesh = AllocateComputeMesh();
				UBlockoutLibrary_GeometryFunctions::GenerateSplineMesh(
					ParapetMesh,
					SplineComp,
					WallThickness,
					0,
					true,
					false
					);
				
				FlatRoofExtrudeOptions.ExtrudeDistance = ParapetHeight;
				UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(
					ParapetMesh,
					FlatRoofExtrudeOptions
					);

				UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
					DynamicMeshComponent->GetDynamicMesh(),
					ParapetMesh,
					FTransform(FVector(0.0f, 0.0f, WallHeight.Z+RoofThickness)),
					false,
					FGeometryScriptAppendMeshOptions()
					);
			}
			break;
		default:
			break;
		}
	}
}
