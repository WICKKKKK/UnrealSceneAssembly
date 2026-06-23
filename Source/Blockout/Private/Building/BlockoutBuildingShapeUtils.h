#pragma once

#include "BlockoutEnum.h"

class AActor;
class UBlockoutSplineComponent;
class UDynamicMesh;
class USplineComponent;

namespace BlockoutBuildingShapeUtils
{
void InitializeSpline(UBlockoutSplineComponent* SplineComp, const TArray<FVector>& Points, bool bClosedLoop, EBlockoutSplinePointType PointType);
USplineComponent* ResolveSpline(AActor* OuterSplineActor, UBlockoutSplineComponent* LocalSplineComp);
void GetSplinePointsWithInterp(USplineComponent* Spline, const FTransform& OwnerTransform, bool bTransformFromWorld, int32 InterpolateNum, TArray<FVector>& OutPoints, FVector& OutCenter);
void AppendPlanarMeshFromPoints(UDynamicMesh* TargetMesh, const TArray<FVector>& Points, EBlockoutVerticalAlignment VerticalAlignment);
void AppendSplineRibbonFromPoints(UDynamicMesh* TargetMesh, const TArray<FVector>& Points, bool bClosedLoop, float Width, EBlockoutAlignment Alignment);
}
