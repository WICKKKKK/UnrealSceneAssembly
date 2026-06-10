#include "Functions/BlockoutLibrary_GeometryFunctions.h"

#include "BlockoutGeometryScriptCompat.h"
#include "BlockoutLog.h"
#include "BlockoutSettings.h"
#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "Components/SplineComponent.h"
#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Functions/BlockoutLibrary_MathFunctions.h"


UDynamicMesh* UBlockoutLibrary_GeometryFunctions::CleanDegenerateTris(
	UDynamicMesh* TargetMesh, float SafeNormalTolerance, bool DebugLog, bool DeleteDegenerates)
{
	if (!UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(TargetMesh))
	{
		return TargetMesh;
	}
	
	FFalconGeometryScriptIndexList TriangleIDList;
	bool bHasTriangleIDGaps;
	UFalconGeometryLibrary_MeshQuery::GetAllTriangleIDs(
		TargetMesh,
		TriangleIDList,
		bHasTriangleIDGaps
		);
	
	TArray<int> DegenerateTriList;
	
	for (int ID : *TriangleIDList.List)
	{
		// UE_LOG(LogBlockout, Warning, TEXT("TriangleID: %d"), ID);
		bool AddToDegenerate = false;
	
		// 计算 SafeNormal 阈值
		bool bIsValidTriangle;
		FVector Vertex1;
		FVector Vertex2;
		FVector Vertex3;
		UFalconGeometryLibrary_MeshQuery::GetTrianglePositions(
			TargetMesh, ID, bIsValidTriangle, Vertex1, Vertex2, Vertex3
			);
		FVector EdgeDir1 = Vertex2 - Vertex1;
		FVector EdgeDir2 = Vertex3 - Vertex1;
		
		if (FVector::CrossProduct(EdgeDir1, EdgeDir2).SizeSquared() < SafeNormalTolerance)
		{
			AddToDegenerate = true;
		}
	
		// 将符合条件的加入到 DegenerateTriList 中
		if (AddToDegenerate)
		{
			DegenerateTriList.Add(ID);
		}
	}
	
	if (DebugLog)
	{
		UE_LOG(LogBlockout, Warning, TEXT("Found %d Degenerate Triangles"), DegenerateTriList.Num());
	}
	
	if (DeleteDegenerates && DegenerateTriList.Num()>0)
	{
		FFalconGeometryScriptIndexList TriangleList;
		TriangleList.Reset(EFalconGeometryScriptIndexType::Triangle);
		TriangleList.List->Append(DegenerateTriList);
		int NumDeleted;
		UFalconGeometryLibrary_MeshBasicEdit::DeleteTrianglesFromMesh(
			TargetMesh,
			TriangleList,
			NumDeleted
			);
		UFalconGeometryLibrary_MeshRepair::CompactMesh(TargetMesh);
	}

	return TargetMesh;
}

UDynamicMesh* UBlockoutLibrary_GeometryFunctions::GeneratePlanarMesh(
	UDynamicMesh* TargetMesh,
	USplineComponent* Spline,
	int InterpolateNum)
{
	TArray<int> SplineIndexes;
	TArray<FVector> SplinePoints;
	TArray<FVector> OutTangentList;
	FVector SplineCenter;
	UBlockoutLibrary_SplineFunctions::GetSplinePointLocationWithInterp(Spline, InterpolateNum,
		SplineIndexes, SplinePoints, OutTangentList, SplineCenter);
	if (SplinePoints.Num() > 2)
	{
		FFalconGeometryScriptPrimitiveOptions PrimitiveOptions;
		FTransform Transform = FTransform::Identity;
		TArray<FVector2D> PolygonVertices;
		for (FVector Pos : SplinePoints)
		{
			PolygonVertices.Add(FVector2D(Pos.X, Pos.Y));
		}
		
		UFalconGeometryLibrary_MeshPrimitive::AppendTriangulatedPolygon(
			TargetMesh,
			PrimitiveOptions,
			Transform,
			PolygonVertices
			);
	}
	return TargetMesh;
}


UDynamicMesh* UBlockoutLibrary_GeometryFunctions::GenerateSplineMesh(
	UDynamicMesh* TargetMesh,
	USplineComponent* Spline,
	float Width,
	int InterpolateNum,
	bool bReverse,
	bool bCenterize
	)
{
	if (!IsValid(Spline))
	{
		return TargetMesh;
	}
	
	bool bIsClosed = Spline->IsClosedLoop();
	int PointNum = Spline->GetNumberOfSplinePoints();
	
	if ((bIsClosed && PointNum>2) || (!bIsClosed && PointNum>1))
	{
		TArray<FVector> LeftOffsetPoints;
		TArray<FVector> RightOffsetPoints;
		if (bCenterize)
		{
			UBlockoutLibrary_SplineFunctions::PolylineOffset(Spline, Width*0.5f, bReverse, InterpolateNum, LeftOffsetPoints);
			UBlockoutLibrary_SplineFunctions::PolylineOffset(Spline, Width*0.5f, !bReverse, InterpolateNum, RightOffsetPoints);
		}else
		{
			TArray<int> SplineIndexes;
			FVector SplineCenter;
			TArray<FVector> OutTangentList;
			UBlockoutLibrary_SplineFunctions::GetSplinePointLocationWithInterp(Spline, InterpolateNum,SplineIndexes,
				LeftOffsetPoints,OutTangentList, SplineCenter);
			UBlockoutLibrary_SplineFunctions::PolylineOffset(Spline, Width, !bReverse, InterpolateNum, RightOffsetPoints);
		}

		for(int i=0; i<LeftOffsetPoints.Num(); ++i)
		{
			int LeftVertexIndex;
			int RightVertexIndex;
			UFalconGeometryLibrary_MeshBasicEdit::AddVertexToMesh(TargetMesh, LeftOffsetPoints[i], LeftVertexIndex);
			UFalconGeometryLibrary_MeshBasicEdit::AddVertexToMesh(TargetMesh, RightOffsetPoints[i], RightVertexIndex);

			FIntVector NewTriangle;
			int NewTriangleIndex;

			if (i > 0)
			{
				if (bReverse)
				{
					NewTriangle = FIntVector((i-1)*2, (i-1)*2+1, LeftVertexIndex);
				}else
				{
					NewTriangle = FIntVector((i-1)*2, LeftVertexIndex, (i-1)*2+1);
				}
				UFalconGeometryLibrary_MeshBasicEdit::AddTriangleToMesh(
					TargetMesh,
					NewTriangle,
					NewTriangleIndex);

				if (bReverse)
				{
					NewTriangle = FIntVector((i-1)*2+1, RightVertexIndex, LeftVertexIndex);
				}else
				{
					NewTriangle = FIntVector((i-1)*2+1, LeftVertexIndex, RightVertexIndex);
				}
				
				UFalconGeometryLibrary_MeshBasicEdit::AddTriangleToMesh(
					TargetMesh,
					NewTriangle,
					NewTriangleIndex);
			}

			if(i == LeftOffsetPoints.Num()-1 && (bIsClosed && PointNum>2))
			{
				if (bReverse)
				{
					NewTriangle = FIntVector(LeftVertexIndex, RightVertexIndex, 0);
				}else
				{
					NewTriangle = FIntVector(LeftVertexIndex, 0, RightVertexIndex);
				}
				
				UFalconGeometryLibrary_MeshBasicEdit::AddTriangleToMesh(
					TargetMesh,
					NewTriangle,
					NewTriangleIndex);

				if (bReverse)
				{
					NewTriangle = FIntVector(RightVertexIndex, 1, 0);
				}else
				{
					NewTriangle = FIntVector(RightVertexIndex, 0, 1);
				}
				
				UFalconGeometryLibrary_MeshBasicEdit::AddTriangleToMesh(
					TargetMesh,
					NewTriangle,
					NewTriangleIndex);
			}
		}
	}
	return TargetMesh;
}

UDynamicMesh* UBlockoutLibrary_GeometryFunctions::GenerateRoofMesh(
	UDynamicMesh* TargetMesh,
	UDynamicMeshPool* MeshPool,
	float RoofHeight,
	float RotateRidgeAngle,
	FVector& RidgeDir)
{
	UDynamicMesh* RotatePlanMesh;
	UDynamicMesh* RoofMesh;
	UDynamicMesh* CutMesh;
	if (MeshPool)
	{
		RotatePlanMesh = MeshPool->RequestMesh();
		RoofMesh = MeshPool->RequestMesh();
		CutMesh = MeshPool->RequestMesh();
	}else
	{
		RotatePlanMesh = NewObject<UDynamicMesh>();
		RoofMesh = NewObject<UDynamicMesh>();
		CutMesh = NewObject<UDynamicMesh>();
	}

	// 计算 roof dir
	FVector RoofMinCorner;
	RidgeDir = FVector::ZeroVector;
	FVector RoofWidthDir = FVector::ZeroVector;
	
	FBox PlanMeshBBox;
	if (RotateRidgeAngle == 0.0f)
	{
		PlanMeshBBox = UFalconGeometryLibrary_MeshQuery::GetMeshBoundingBox(TargetMesh);
	}else
	{
		UFalconGeometryLibrary_MeshDecomposition::CopyMeshToMesh(
			TargetMesh,
			RotatePlanMesh,
			RotatePlanMesh);
		
		UFalconGeometryLibrary_MeshTransform::TransformMesh(
			RotatePlanMesh,
			FTransform(FQuat(FRotator(0.0f, RotateRidgeAngle, 0.0f))));

		PlanMeshBBox = UFalconGeometryLibrary_MeshQuery::GetMeshBoundingBox(RotatePlanMesh);
	}
	RoofMinCorner = PlanMeshBBox.Min;
	RidgeDir.Y = PlanMeshBBox.Max.Y - PlanMeshBBox.Min.Y;
	RoofWidthDir.X = PlanMeshBBox.Max.X - PlanMeshBBox.Min.X;

	// 生成山墙面 Mesh
	TArray<FVector2D> MountainWallVertices;
	MountainWallVertices.Add(FVector2D(0.0f, 0.0f));
	float MountainWallWidth = FMath::Max(RoofWidthDir.X, RoofWidthDir.Y);
	MountainWallVertices.Add(FVector2D(MountainWallWidth*0.5f, RoofHeight));
	MountainWallVertices.Add(FVector2D(MountainWallWidth, 0.0f));
	UFalconGeometryLibrary_MeshPrimitive::AppendTriangulatedPolygon(
		RoofMesh,
		FFalconGeometryScriptPrimitiveOptions(),
		FTransform(FQuat(FRotator(0.0f, 0.0f, -90.0f))),
		MountainWallVertices
		);

	FTransform RoofTransform;
	RoofTransform.SetLocation(RoofMinCorner);
	RoofTransform.SetRotation(FQuat(UBlockoutLibrary_MathFunctions::GetRotFromTwoDir(
		FVector(1.0f, 0.0f, 0.0f),  RoofWidthDir)));
	UFalconGeometryLibrary_MeshTransform::TransformMesh(RoofMesh, RoofTransform);

	bool bIsValidTriangle;
	FVector MountainWallFaceNormal = UFalconGeometryLibrary_MeshQuery::GetTriangleFaceNormal(RoofMesh, 0, bIsValidTriangle);
	if (FVector::DotProduct(MountainWallFaceNormal, RidgeDir) < 0.0f)
	{
		UFalconGeometryLibrary_MeshNormal::FlipNormals(RoofMesh);
	}

	// 挤出 Roof Mesh
	FFalconGeometryScriptMeshExtrudeOptions RoofExtrudeOptions;
	RoofExtrudeOptions.ExtrudeDirection = RidgeDir;
	UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(RoofMesh, RoofExtrudeOptions);

	if (!RotateRidgeAngle==0.0f)
	{
		UFalconGeometryLibrary_MeshTransform::TransformMesh(
			RoofMesh,
			FTransform(FQuat(FRotator(0.0f, RotateRidgeAngle*-1.0f, 0.0f))));

		RidgeDir = FRotator(0.0f, RotateRidgeAngle, 0.0f).RotateVector(RidgeDir);
	}

	// // 切掉多余的 Mesh
	// UFalconGeometryLibrary_MeshDecomposition::CopyMeshToMesh(TargetMesh, CutMesh, CutMesh);
	// UFalconGeometryLibrary_MeshTransform::TranslateMesh(CutMesh, FVector(0.0f, 0.0f, -1.0f));
	// FFalconGeometryScriptMeshExtrudeOptions CutExtrudeOptions;
	// CutExtrudeOptions.ExtrudeDistance = RoofHeight + 2.0f;
	// UFalconGeometryLibrary_MeshModeling::ApplyMeshExtrude_Compatibility_5p0(CutMesh, CutExtrudeOptions);
	// FFalconGeometryScriptEdgeData Edges;
	// UFalconGeometryLibrary_MeshBoolean::ApplyMeshBoolean(
	// 	RoofMesh,
	// 	FTransform::Identity,
	// 	CutMesh,
	// 	FTransform::Identity,
	// 	EFalconGeometryScriptBooleanOperation::Intersection,
	// 	FFalconGeometryScriptMeshBooleanOptions(),
	// 	Edges
	// 	);

	// UFalconGeometryLibrary_MeshBasicEditFunctions::AppendMesh(
	// 	TargetMesh,
	// 	RoofMesh,
	// 	FTransform::Identity
	// 	);
	
	TargetMesh->SetMesh(RoofMesh->GetMeshRef());
	if (MeshPool)
	{
		MeshPool->ReturnMesh(RotatePlanMesh);
		MeshPool->ReturnMesh(RoofMesh);
		MeshPool->ReturnMesh(CutMesh);
	}
	
	return TargetMesh;
}
