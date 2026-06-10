#include "Building/BlockoutHouse.h"

#include "BlockoutGeometryScriptCompat.h"
#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "Functions/BlockoutLibrary_GeometryFunctions.h"
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
		
		UFalconGeometryLibrary_MeshDecomposition::CopyMeshToMesh(
			PlanMesh,
			BaseFloorMesh,
			BaseFloorMesh
			);

		FFalconGeometryScriptMeshExtrudeOptions FloorExtrudeOptions;
		FloorExtrudeOptions.ExtrudeDistance = BaseFloorThickness.Z;
		UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(
			BaseFloorMesh,
			FloorExtrudeOptions
			);

		UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
			DynamicMeshComponent->GetDynamicMesh(),
			BaseFloorMesh,
			FTransform::Identity,
			FFalconGeometryScriptAppendMeshOptions()
			);

		if (bGenerateMiddleFloor)
		{
			if (FloorNum >= 2)
			{
				UDynamicMesh* MiddleFloorMesh = AllocateComputeMesh();
				UFalconGeometryLibrary_MeshDecomposition::CopyMeshToMesh(
					PlanMesh,
					MiddleFloorMesh,
					MiddleFloorMesh
					);

				FloorExtrudeOptions.ExtrudeDistance = MiddleFloorThickness;
				UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(
					MiddleFloorMesh,
					FloorExtrudeOptions
					);

				UFalconGeometryLibrary_MeshTransform::TranslateMesh(
					MiddleFloorMesh,
					FVector(0.0f, 0.0f, BaseFloorThickness.Z)
					);

				UFalconGeometryLibrary_MeshBasicEdit::AppendMeshRepeated(
					DynamicMeshComponent->GetDynamicMesh(),
					MiddleFloorMesh,
					FTransform(FVector(0.0f, 0.0f, (WallHeight.Z-BaseFloorThickness.Z)/float(FloorNum))),
					FFalconGeometryScriptAppendMeshOptions(),
					FloorNum-1
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

		FFalconGeometryScriptMeshExtrudeOptions WallExtrudeOptions;
		WallExtrudeOptions.ExtrudeDistance = WallHeight.Z - BaseFloorThickness.Z;
		UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(
			WallMesh,
			WallExtrudeOptions
			);

		UFalconGeometryLibrary_MeshTransform::TranslateMesh(
			WallMesh,
			FVector(0.0f, 0.0f, BaseFloorThickness.Z)
			);

		if (bGenerateHoles)
		{
			HoleSize.Y = HoleSize.X;
			
			UDynamicMesh* AllWindowMesh = AllocateComputeMesh();
			UDynamicMesh* WindowMesh = AllocateComputeMesh();

			FFalconGeometryScriptPrimitiveOptions PrimitiveOptions;
			UFalconGeometryLibrary_MeshPrimitive::AppendBox(
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
				UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
					AllWindowMesh,
					WindowMesh,
					AppendTransform,
					FFalconGeometryScriptAppendMeshOptions()
					);
			}

			UFalconGeometryLibrary_MeshBasicEdit::AppendMeshRepeated(
				AllWindowMesh,
				AllWindowMesh,
				FTransform(FVector(0.0f, 0.0f, (WallHeight.Z-BaseFloorThickness.Z)/float(FloorNum))),
				FFalconGeometryScriptAppendMeshOptions(),
				FloorNum-1
				);
			FFalconGeometryScriptEdgeData Edges;
			UFalconGeometryLibrary_MeshBoolean::ApplyMeshBoolean(
				WallMesh,
				FTransform::Identity,
				AllWindowMesh,
				FTransform(FVector(0.0f, 0.0f, HoleHeightOffset)),
				EFalconGeometryScriptBooleanOperation::Subtract,
				FFalconGeometryScriptMeshBooleanOptions(),
				Edges
				);
		}
		UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
			DynamicMeshComponent->GetDynamicMesh(),
			WallMesh,
			FTransform::Identity,
			FFalconGeometryScriptAppendMeshOptions()
			);
	}

	if (bGenerateRoof)
	{
		UDynamicMesh* RoofMesh = AllocateComputeMesh();
		UFalconGeometryLibrary_MeshDecomposition::CopyMeshToMesh(
			PlanMesh,
			RoofMesh,
			RoofMesh
			);

		FVector RidgeDir;
		FFalconGeometryScriptMeshExtrudeOptions FlatRoofExtrudeOptions;
		
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

			UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
				DynamicMeshComponent->GetDynamicMesh(),
				RoofMesh,
				FTransform(FVector(0.0f, 0.0f, WallHeight.Z)),
				FFalconGeometryScriptAppendMeshOptions()
				);
			break;
		case EHouseRoofType::FlatRoof:
			FlatRoofExtrudeOptions.ExtrudeDistance = RoofThickness;
			UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(
				RoofMesh,
				FlatRoofExtrudeOptions
				);
			
			UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
				DynamicMeshComponent->GetDynamicMesh(),
				RoofMesh,
				FTransform(FVector(0.0f, 0.0f, WallHeight.Z)),
				FFalconGeometryScriptAppendMeshOptions()
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
				UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(
					ParapetMesh,
					FlatRoofExtrudeOptions
					);

				UFalconGeometryLibrary_MeshBasicEdit::AppendMesh(
					DynamicMeshComponent->GetDynamicMesh(),
					ParapetMesh,
					FTransform(FVector(0.0f, 0.0f, WallHeight.Z+RoofThickness)),
					FFalconGeometryScriptAppendMeshOptions()
					);
			}
			break;
		default:
			break;
		}
	}
}
