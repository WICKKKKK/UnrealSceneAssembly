#include "Functions/BlockoutLibrary_FaceFunctions.h"

#include "Functions/BlockoutLibrary_EditorFunctions.h"


TArray<FBlockoutFace> UBlockoutLibrary_FaceFunctions::GetBoxFaces(const FBox& Box, const FTransform& Transform)
{
	FOrientedBox OrientedBox = FOrientedBox();
	OrientedBox.Center = Transform.TransformPosition(Box.GetCenter());
	OrientedBox.AxisX = Transform.TransformVectorNoScale(FVector(1.0f, 0.0f, 0.0f)).GetSafeNormal();
	OrientedBox.AxisY = Transform.TransformVectorNoScale(FVector(0.0f, 1.0f, 0.0f)).GetSafeNormal();
	OrientedBox.AxisZ = Transform.TransformVectorNoScale(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
	FVector Extent = Box.GetExtent() * Transform.GetScale3D();
	OrientedBox.ExtentX = Extent.X;
	OrientedBox.ExtentY = Extent.Y;
	OrientedBox.ExtentZ = Extent.Z;

	TArray<FBlockoutFace> OutFaces;
	OutFaces.Reset();

	// 前面
	OutFaces.Add({OrientedBox.Center - OrientedBox.AxisX * OrientedBox.ExtentX,
		-OrientedBox.AxisX, OrientedBox.AxisZ * OrientedBox.ExtentZ,
		OrientedBox.AxisY * OrientedBox.ExtentY}
	);

	// 后面
	OutFaces.Add({OrientedBox.Center + OrientedBox.AxisX * OrientedBox.ExtentX,
		OrientedBox.AxisX, OrientedBox.AxisY * OrientedBox.ExtentY,
		OrientedBox.AxisZ * OrientedBox.ExtentZ}
	);

	// 右面
	OutFaces.Add({OrientedBox.Center + OrientedBox.AxisY * OrientedBox.ExtentY,
		OrientedBox.AxisY, OrientedBox.AxisZ * OrientedBox.ExtentZ,
		OrientedBox.AxisX * OrientedBox.ExtentX}
	);

	// 左面
	OutFaces.Add({OrientedBox.Center - OrientedBox.AxisY * OrientedBox.ExtentY,
		-OrientedBox.AxisY, OrientedBox.AxisX * OrientedBox.ExtentX,
		OrientedBox.AxisZ * OrientedBox.ExtentZ}
	);

	// 上面
	OutFaces.Add({OrientedBox.Center + OrientedBox.AxisZ * OrientedBox.ExtentZ,
		OrientedBox.AxisZ, OrientedBox.AxisX * OrientedBox.ExtentX,
		OrientedBox.AxisY * OrientedBox.ExtentY}
	);

	// 下面
	OutFaces.Add({OrientedBox.Center - OrientedBox.AxisZ * OrientedBox.ExtentZ,
		-OrientedBox.AxisZ, OrientedBox.AxisY * OrientedBox.ExtentY,
		OrientedBox.AxisX * OrientedBox.ExtentX}
	);

	return OutFaces;
}

FVector UBlockoutLibrary_FaceFunctions::ProjectPointToFace(const FVector& InPoint, const FBlockoutFace& TargetFace,
                                                           bool& bInside, float& ProjectDistance)
{
	const FVector TargetFaceNormal = TargetFace.Normal.GetSafeNormal();

	// 计算点投影到平面上的位置
	const FVector V = InPoint - TargetFace.Origin;
	ProjectDistance = FVector::DotProduct(V, TargetFaceNormal);
	const FVector ProjectionPoint = InPoint - ProjectDistance * TargetFaceNormal;
	ProjectDistance = FMath::Abs(ProjectDistance);

	// 判断投影点是否在面内
	const FVector VecToProjection = ProjectionPoint - TargetFace.Origin;
	const float XAxisLengthSq = TargetFace.XAxis.SizeSquared();
	const float YAxisLengthSq = TargetFace.YAxis.SizeSquared();
	// 计算局部坐标参数u和v
	const float u = FVector::DotProduct(VecToProjection, TargetFace.XAxis) / XAxisLengthSq;
	const float v = FVector::DotProduct(VecToProjection, TargetFace.YAxis) / YAxisLengthSq;
	
	bInside = (FMath::Abs(u) <= 1.0f) && (FMath::Abs(v) <= 1.0f);
	
	return ProjectionPoint;
}

FVector UBlockoutLibrary_FaceFunctions::ProjectDirectionToFace(const FVector& InDirection,
	const FBlockoutFace& TargetFace)
{
	FVector TargetNormal = TargetFace.Normal.GetSafeNormal();
	if (TargetNormal.IsZero()) return FVector::ZeroVector;
	
	FVector ProjectedDirection = InDirection - (FVector::DotProduct(InDirection, TargetNormal)) * TargetNormal;
	return ProjectedDirection;
}

FBlockoutFace UBlockoutLibrary_FaceFunctions::RotateFaceByOrigin(const FBlockoutFace& TargetFace, const FQuat& Rotation)
{
	FBlockoutFace RotatedFace = TargetFace;
	RotatedFace.XAxis = Rotation.RotateVector(RotatedFace.XAxis);
	RotatedFace.YAxis = Rotation.RotateVector(RotatedFace.YAxis);
	RotatedFace.Normal = Rotation.RotateVector(RotatedFace.Normal);
	// UBlockoutLibrary_EditorFunctions::DrawDebugBlockoutFace(RotatedFace, FColor::Red, 0.01f, 1.0f);

	return RotatedFace;
}

bool UBlockoutLibrary_FaceFunctions::AreFacesParallel(const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace,
                                                      const float AngleThreshold)
{
	const float DotProduct = FVector::DotProduct(TargetFace.Normal.GetSafeNormal(), OtherFace.Normal.GetSafeNormal());
	return FMath::Abs(DotProduct) >= FMath::Cos(FMath::DegreesToRadians(AngleThreshold));
}

float UBlockoutLibrary_FaceFunctions::CalculateProjectionDistanceBetweenFaces(const FBlockoutFace& TargetFace,
	const FBlockoutFace& OtherFace)
{
	const float ProjectionDistance = FVector::DotProduct(TargetFace.Origin - OtherFace.Origin, OtherFace.Normal.GetSafeNormal());
	return FMath::Abs(ProjectionDistance);
}


FQuat UBlockoutLibrary_FaceFunctions::CalculateAlignFaceNormalRotation(const FVector& TargetDirection,
	const FBlockoutFace& OtherFace)
{
	// 归一化法线
	const FVector TargetDirectionNormalized = TargetDirection.GetSafeNormal();
	const FVector OtherNormal = OtherFace.Normal.GetSafeNormal();
	if (TargetDirectionNormalized.IsZero() || OtherNormal.IsZero()) return FQuat::Identity;

	// 候选法线列表（包括正负轴）
	TArray<FVector> CandidateNormals = { OtherNormal, -OtherNormal };
	float MaxNormalDot = -1.0f;
	FVector BestNormal = OtherNormal;
	for (const FVector& Normal : CandidateNormals)
	{
		float Dot = FVector::DotProduct(Normal, TargetDirectionNormalized);
		if (Dot > MaxNormalDot)
		{
			MaxNormalDot = Dot;
			BestNormal = Normal;
		}
	}

	// 对齐法线方向, 如果要计算当前旋转角度, 可用 RotateToNormal.GetAngle(), 得到的是弧度
	FQuat RotateToNormal = FQuat::FindBetweenNormals(TargetDirectionNormalized, BestNormal);

	return RotateToNormal;
}

bool UBlockoutLibrary_FaceFunctions::FindClosestFaceAxis(const FVector& TargetDirection,
	const FBlockoutFace& OtherFace, FVector& OutClosestAxis, FVector& OutProjectedDirection)
{
	// 归一化法线，确保方向正确
	const FVector OtherNormal = OtherFace.Normal.GetSafeNormal();
	if (OtherNormal.IsZero()) return false;

	// 将 TargetDirection 投影到面平面上
	OutProjectedDirection = ProjectDirectionToFace(TargetDirection, OtherFace);
	FVector ProjectedDirectionNormalized = OutProjectedDirection.GetSafeNormal();
	if (ProjectedDirectionNormalized.IsZero()) return false;

	// 获取面B平面内的四个候选轴方向（+X, +Y, -X, -Y）
	FVector BX = OtherFace.XAxis.GetSafeNormal();
	FVector BY = OtherFace.YAxis.GetSafeNormal();
	if (BX.IsZero() || BY.IsZero()) return false;

	// 候选方向列表（包括正负轴）
	TArray<FVector> CandidateAxes = { BX, BY, -BX, -BY };

	float BXLenght = OtherFace.XAxis.Size();
	float BYLenght = OtherFace.YAxis.Size();
	TArray<float> CandidateExtents = { BXLenght, BYLenght, BXLenght, BYLenght };
	
	// 找到与投影方向最接近的候选轴
	float MaxAxeDot = -1.0f;
	OutClosestAxis = BX;
	int ClosestAxisIndex = 0;
	for (int i = 0; i < CandidateAxes.Num(); i++)
	{
		float Dot = FVector::DotProduct(ProjectedDirectionNormalized, CandidateAxes[i]);
		if (Dot > MaxAxeDot)
		{
			MaxAxeDot = Dot;
			OutClosestAxis = CandidateAxes[i];
			ClosestAxisIndex = i;
		}
	}

	OutClosestAxis *= CandidateExtents[ClosestAxisIndex];

	return true;
}

bool UBlockoutLibrary_FaceFunctions::CalculateMinEdgeDistanceBetweenFacesInDirection(const FBlockoutFace& TargetFace,
	const FBlockoutFace& OtherFace, const FVector& Direction, float& MinEdgeDistance, FVector& OutTargetAxis,
	FVector& OutOtherAxis)
{
	if (TargetFace.XAxis.IsNearlyZero() || TargetFace.YAxis.IsNearlyZero()) return false;
	if (OtherFace.XAxis.IsNearlyZero() || OtherFace.YAxis.IsNearlyZero()) return false;
	FVector DirectionNormalized = Direction.GetSafeNormal();
	if (DirectionNormalized.IsZero()) return false;

	FVector TargetAxis = TargetFace.XAxis;
	if (FMath::Abs(FVector::DotProduct(DirectionNormalized, TargetFace.YAxis.GetSafeNormal())) >
		FMath::Abs(FVector::DotProduct(DirectionNormalized, TargetFace.XAxis.GetSafeNormal())))
	{
		TargetAxis = TargetFace.YAxis;
	}
	FVector OtherAxis = OtherFace.XAxis;
	if (FMath::Abs(FVector::DotProduct(DirectionNormalized, OtherFace.YAxis.GetSafeNormal())) >
		FMath::Abs(FVector::DotProduct(DirectionNormalized, OtherFace.XAxis.GetSafeNormal())))
	{
		OtherAxis = OtherFace.YAxis;
	}

	TArray<FVector> TargetUsedAxes = { TargetAxis, -TargetAxis };
	TArray<FVector> OtherUsedAxes = { OtherAxis, -OtherAxis };

	MinEdgeDistance = TNumericLimits<float>::Max();
	for (const FVector& TargetUsedAxis : TargetUsedAxes)
	{
		for (const FVector& OtherUsedAxis : OtherUsedAxes)
		{
			FVector OtherEdgeCenter = OtherFace.Origin + OtherUsedAxis;
			FVector TargetEdgeCenter = TargetFace.Origin + TargetUsedAxis;
			float EdgeDistance = FMath::Abs(FVector::DotProduct(OtherEdgeCenter - TargetEdgeCenter, OtherUsedAxis.GetSafeNormal()));

			if (EdgeDistance < MinEdgeDistance)
			{
				MinEdgeDistance = EdgeDistance;
				OutTargetAxis = TargetUsedAxis;
				OutOtherAxis = OtherUsedAxis;
			}
		}
	}

	return true;
}


FQuat UBlockoutLibrary_FaceFunctions::CalculateAlignFaceAxisRotation(const FVector& TargetDirection,
                                                                     const FBlockoutFace& OtherFace)
{
	FVector ClosestAxis;
	FVector TargetDirectionProjected;
	bool bFoundAxis = FindClosestFaceAxis(TargetDirection, OtherFace, ClosestAxis, TargetDirectionProjected);
	if (!bFoundAxis) return FQuat::Identity;
	
	// 计算绕法线旋转以对齐最佳轴, 如果要计算当前旋转角度, 可用 RotateInPlane.GetAngle(), 得到的是弧度
	FQuat RotateInPlane = FQuat::FindBetweenVectors(TargetDirectionProjected.GetSafeNormal(), ClosestAxis.GetSafeNormal());

	return RotateInPlane;
}

bool UBlockoutLibrary_FaceFunctions::CalculateMinEdgeDistanceBetweenFaces(
	const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace, const float EdgeSnapThreshold,
	float& OutMinEdgeDistance, FVector& OutTargetEdgeCenter, FVector& OutOtherAxis)
{
	// TODO: 优化算法性能
	FVector AX = TargetFace.XAxis;
	FVector AY = TargetFace.YAxis;
	if (AX.IsZero() || AY.IsZero()) return false;

	FVector BX = OtherFace.XAxis;
	FVector BY = OtherFace.YAxis;
	if (BX.IsZero() || BY.IsZero()) return false;

	TArray<FVector> AllTargetAxes = { AX, AY, -AX, -AY };
	OutMinEdgeDistance = EdgeSnapThreshold;
	bool bFoundEdge = false;
	
	for (const FVector& TargetAxis : AllTargetAxes)
	{
		TArray<FVector> OtherAxes = { BX, BY };
		
		for (const FVector& OtherAxis : OtherAxes)
		{
			FVector ChooseAxis = BX;
			if (FMath::Abs(FVector::DotProduct(TargetAxis.GetSafeNormal(), BY.GetSafeNormal())) >
				FMath::Abs(FVector::DotProduct(TargetAxis.GetSafeNormal(), BX.GetSafeNormal())))
			{
				ChooseAxis = BY;
			}
			TArray<FVector> OtherUsedAxes = { ChooseAxis, -ChooseAxis };
			for (const FVector& OtherUsedAxis : OtherUsedAxes)
			{
				FVector OtherEdgeCenter = OtherFace.Origin + OtherUsedAxis;
				FVector TargetEdgeCenter = TargetFace.Origin + TargetAxis;
				float EdgeDistance = FMath::Abs(FVector::DotProduct(OtherEdgeCenter - TargetEdgeCenter, OtherUsedAxis.GetSafeNormal()));

				if (EdgeDistance < OutMinEdgeDistance)
				{
					OutMinEdgeDistance = EdgeDistance;
					OutTargetEdgeCenter = TargetEdgeCenter;
					OutOtherAxis = OtherUsedAxis;
					bFoundEdge = true;
				}
			}
		}
	}

	return bFoundEdge;
}
