#include "BlockoutBuildingShapeUtils.h"

#include "Components/BlockoutSplineComponent.h"
#include "Components/SplineComponent.h"
#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "GameFramework/Actor.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

namespace BlockoutBuildingShapeUtils
{
void InitializeSpline(UBlockoutSplineComponent* SplineComp, const TArray<FVector>& Points, bool bClosedLoop, EBlockoutSplinePointType PointType)
{
	if (!IsValid(SplineComp))
	{
		return;
	}

	SplineComp->SetSplinePoints(Points, ESplineCoordinateSpace::Local, false);
	SplineComp->SetClosedLoop(bClosedLoop, false);
	UBlockoutLibrary_SplineFunctions::SetSplinePointType(SplineComp, PointType);
	SplineComp->UpdateSpline();
}

USplineComponent* ResolveSpline(AActor* OuterSplineActor, UBlockoutSplineComponent* LocalSplineComp)
{
	if (IsValid(OuterSplineActor))
	{
		if (USplineComponent* OuterSpline = OuterSplineActor->FindComponentByClass<USplineComponent>())
		{
			return OuterSpline;
		}
	}

	return LocalSplineComp;
}

void GetSplinePointsWithInterp(USplineComponent* Spline, const FTransform& OwnerTransform, bool bTransformFromWorld, int32 InterpolateNum, TArray<FVector>& OutPoints, FVector& OutCenter)
{
	OutPoints.Reset();
	OutCenter = FVector::ZeroVector;

	if (!IsValid(Spline))
	{
		return;
	}

	const int32 PointNum = Spline->GetNumberOfSplinePoints();
	if (PointNum <= 0)
	{
		return;
	}

	const bool bClosedLoop = Spline->IsClosedLoop();
	const ESplineCoordinateSpace::Type Space = bTransformFromWorld ? ESplineCoordinateSpace::World : ESplineCoordinateSpace::Local;
	const FTransform WorldToOwner = OwnerTransform.Inverse();

	auto ConvertPoint = [&](const FVector& Point)
	{
		return bTransformFromWorld ? WorldToOwner.TransformPosition(Point) : Point;
	};

	for (int32 Index = 0; Index < PointNum; ++Index)
	{
		const FVector CurPoint = ConvertPoint(Spline->GetLocationAtSplinePoint(Index, Space));
		OutPoints.Add(CurPoint);
		OutCenter += CurPoint;

		const bool bHasNext = bClosedLoop || Index < PointNum - 1;
		if (bHasNext && InterpolateNum > 0)
		{
			const float SegmentTime = bClosedLoop ? 1.0f / float(PointNum) : 1.0f / float(PointNum - 1);
			const float CurTime = bClosedLoop ? float(Index) / float(PointNum) : float(Index) / float(PointNum - 1);
			const float InterpSegmentTime = SegmentTime / float(InterpolateNum + 1);

			for (int32 InterpIndex = 1; InterpIndex < InterpolateNum + 1; ++InterpIndex)
			{
				const float CurInterpTime = float(InterpIndex) * InterpSegmentTime + CurTime;
				OutPoints.Add(ConvertPoint(Spline->GetLocationAtTime(CurInterpTime, Space, false)));
			}
		}
	}

	OutCenter /= float(PointNum);
}

void AppendPlanarMeshFromPoints(UDynamicMesh* TargetMesh, const TArray<FVector>& Points, EBlockoutVerticalAlignment VerticalAlignment)
{
	if (!IsValid(TargetMesh) || Points.Num() <= 2)
	{
		return;
	}

	float ZOffset = Points[0].Z;
	TArray<FVector2D> PolygonVertices;
	PolygonVertices.Reserve(Points.Num());

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector& Point = Points[Index];
		PolygonVertices.Add(FVector2D(Point.X, Point.Y));

		if (Index == 0)
		{
			continue;
		}

		if (VerticalAlignment == EBlockoutVerticalAlignment::Bottom)
		{
			ZOffset = FMath::Min(ZOffset, Point.Z);
		}
		else if (VerticalAlignment == EBlockoutVerticalAlignment::Top)
		{
			ZOffset = FMath::Max(ZOffset, Point.Z);
		}
		else
		{
			ZOffset += Point.Z;
		}
	}

	if (VerticalAlignment == EBlockoutVerticalAlignment::Center)
	{
		ZOffset /= float(Points.Num());
	}

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTriangulatedPolygon(
		TargetMesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(FVector(0.0f, 0.0f, ZOffset)),
		PolygonVertices,
		true);
}

void AppendSplineRibbonFromPoints(UDynamicMesh* TargetMesh, const TArray<FVector>& Points, bool bClosedLoop, float Width, EBlockoutAlignment Alignment)
{
	if (!IsValid(TargetMesh) || ((bClosedLoop && Points.Num() <= 2) || (!bClosedLoop && Points.Num() <= 1)))
	{
		return;
	}

	const float ClampedWidth = FMath::Max(Width, 0.01f);
	TArray<FVector> LeftOffsetPoints;
	TArray<FVector> RightOffsetPoints;

	if (Alignment == EBlockoutAlignment::Center)
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(Points, bClosedLoop, ClampedWidth * 0.5f, false, LeftOffsetPoints);
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(Points, bClosedLoop, ClampedWidth * 0.5f, true, RightOffsetPoints);
	}
	else if (Alignment == EBlockoutAlignment::Left)
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(Points, bClosedLoop, ClampedWidth, false, LeftOffsetPoints);
		RightOffsetPoints = Points;
	}
	else
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(Points, bClosedLoop, ClampedWidth, true, RightOffsetPoints);
		LeftOffsetPoints = Points;
	}

	if (LeftOffsetPoints.Num() != Points.Num() || RightOffsetPoints.Num() != Points.Num())
	{
		return;
	}

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		int32 LeftVertexIndex;
		int32 RightVertexIndex;
		UGeometryScriptLibrary_MeshBasicEditFunctions::AddVertexToMesh(TargetMesh, LeftOffsetPoints[Index], LeftVertexIndex);
		UGeometryScriptLibrary_MeshBasicEditFunctions::AddVertexToMesh(TargetMesh, RightOffsetPoints[Index], RightVertexIndex);

		if (Index > 0)
		{
			int32 NewTriangleIndex;
			UGeometryScriptLibrary_MeshBasicEditFunctions::AddTriangleToMesh(TargetMesh, FIntVector((Index - 1) * 2, LeftVertexIndex, (Index - 1) * 2 + 1), NewTriangleIndex);
			UGeometryScriptLibrary_MeshBasicEditFunctions::AddTriangleToMesh(TargetMesh, FIntVector((Index - 1) * 2 + 1, LeftVertexIndex, RightVertexIndex), NewTriangleIndex);
		}

		if (Index == Points.Num() - 1 && bClosedLoop)
		{
			int32 NewTriangleIndex;
			UGeometryScriptLibrary_MeshBasicEditFunctions::AddTriangleToMesh(TargetMesh, FIntVector(LeftVertexIndex, 0, RightVertexIndex), NewTriangleIndex);
			UGeometryScriptLibrary_MeshBasicEditFunctions::AddTriangleToMesh(TargetMesh, FIntVector(RightVertexIndex, 0, 1), NewTriangleIndex);
		}
	}
}
}
