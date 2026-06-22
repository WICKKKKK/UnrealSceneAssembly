#include "Shape/BlockoutArch.h"

#include "Functions/BlockoutLibrary_MathFunctions.h"
#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"

namespace
{
void AppendOpenSplineRibbon(UDynamicMesh* TargetMesh, const TArray<FVector>& SplinePoints, float Width, EBlockoutAlignment Alignment)
{
	if (!IsValid(TargetMesh) || SplinePoints.Num() <= 1)
	{
		return;
	}

	TArray<FVector> LeftOffsetPoints;
	TArray<FVector> RightOffsetPoints;
	const bool bIsClosed = false;
	const float ClampedWidth = FMath::Max(Width, 1.0f);

	if (Alignment == EBlockoutAlignment::Center)
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(SplinePoints, bIsClosed, ClampedWidth * 0.5f, false, LeftOffsetPoints);
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(SplinePoints, bIsClosed, ClampedWidth * 0.5f, true, RightOffsetPoints);
	}
	else if (Alignment == EBlockoutAlignment::Left)
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(SplinePoints, bIsClosed, ClampedWidth, false, LeftOffsetPoints);
		RightOffsetPoints = SplinePoints;
	}
	else
	{
		UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(SplinePoints, bIsClosed, ClampedWidth, true, RightOffsetPoints);
		LeftOffsetPoints = SplinePoints;
	}

	for (int32 Index = 0; Index < SplinePoints.Num(); ++Index)
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
	}
}
}

ABlockoutArch::ABlockoutArch()
{
}

void ABlockoutArch::CPPGenerateBlockoutMesh()
{
	TArray<FVector> Positions;
	TArray<FVector> Tangents;
	UBlockoutLibrary_MathFunctions::AnalysePointsFromEllipse(
		FMath::Max(Angle, 1.0f),
		0.0f,
		FMath::Max(Sections, 1),
		FMath::Max(XRadius, 1.0f),
		FMath::Max(YRadius, 1.0f),
		bReversed,
		Positions,
		Tangents);

	UDynamicMesh* TargetMesh = DynamicMeshComponent->GetDynamicMesh();
	AppendOpenSplineRibbon(TargetMesh, Positions, Thickness, Alignment);

	FTransform Transform;
	Transform.SetRotation(FQuat(FRotator(90.0f, 0.0f, 0.0f)));
	UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(TargetMesh, Transform);
	UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(TargetMesh);

	FGeometryScriptMeshExtrudeOptions ExtrudeOptions;
	ExtrudeOptions.ExtrudeDistance = FMath::Max(ExtrudeDistance, 1.0f);
	ExtrudeOptions.ExtrudeDirection = FVector(1.0f, 0.0f, 0.0f);
	ExtrudeOptions.UVScale = 1.0f;
	ExtrudeOptions.bSolidsToShells = true;
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshExtrude_Compatibility_5p0(TargetMesh, ExtrudeOptions);
}
