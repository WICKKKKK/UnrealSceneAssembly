#include "Functions/BlockoutLibrary_MathFunctions.h"

#include "Functions/BlockoutLibrary_BasicFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SplineMeshComponent.h"


float UBlockoutLibrary_MathFunctions::BlockoutFloatToFloat(FBlockoutFloat InFloat, float Module)
{
	if (Module > 0.0f)
	{
		return FMath::RoundToFloat(InFloat.Value / Module) * Module;
	}
	
	return InFloat.Value;
}

int UBlockoutLibrary_MathFunctions::BlockoutIntToInt(FBlockoutInt InInt, int Module)
{
	if (Module > 0)
	{
		return InInt.Value / Module * Module;
	}
	return InInt.Value;
}

FVector UBlockoutLibrary_MathFunctions::BlockoutVectorToVector(FBlockoutFVector InVector, FVector Module)
{
	FVector OutVector;
	OutVector.X = BlockoutFloatToFloat(InVector.X, Module.X);
	OutVector.Y = BlockoutFloatToFloat(InVector.Y, Module.Y);
	OutVector.Z = BlockoutFloatToFloat(InVector.Z, Module.Z);
	return OutVector;
}

FVector2D UBlockoutLibrary_MathFunctions::BlockoutVector2DToVector2D(FBlockoutFVector2D InVector2D, FVector2D Module)
{
	FVector2D OutVector2D;
	OutVector2D.X = BlockoutFloatToFloat(InVector2D.X, Module.X);
	OutVector2D.Y = BlockoutFloatToFloat(InVector2D.Y, Module.Y);
	return OutVector2D;
}

FVector4 UBlockoutLibrary_MathFunctions::BlockoutVector4ToVector4(FBlockoutFVector4 InVector4,
	FVector4 Module = FVector4(0.01f, 0.01f, 0.01f, 0.01f))
{
	FVector4 OutVector4;
	OutVector4.X = BlockoutFloatToFloat(InVector4.X, Module.X);
	OutVector4.Y = BlockoutFloatToFloat(InVector4.Y, Module.Y);
	OutVector4.Z = BlockoutFloatToFloat(InVector4.Z, Module.Z);
	OutVector4.W = BlockoutFloatToFloat(InVector4.W, Module.W);
	return OutVector4;
}

FIntVector UBlockoutLibrary_MathFunctions::BlockoutIntVectorToIntVector(FBlockoutFIntVector InIntVector,
	FIntVector Module = FIntVector(1, 1, 1))
{
	FIntVector OutIntVector;
	OutIntVector.X = BlockoutIntToInt(InIntVector.X, Module.X);
	OutIntVector.Y = BlockoutIntToInt(InIntVector.Y, Module.Y);
	OutIntVector.Z = BlockoutIntToInt(InIntVector.Z, Module.Z);
	return OutIntVector;
}

void UBlockoutLibrary_MathFunctions::BlockoutIntVector2DToIntVector2D(FBlockoutFIntVector2D InIntVector2D, int& OutX, int& OutY,
	int ModuleX, int ModuleY)
{
	OutX = BlockoutIntToInt(InIntVector2D.X, ModuleX);
	OutY = BlockoutIntToInt(InIntVector2D.Y, ModuleY);
}

float UBlockoutLibrary_MathFunctions::FloatToModuleFloat(float InFloat, float Module)
{
	if (Module > 0.0f)
	{
		return FMath::RoundToFloat(InFloat / Module) * Module;
	}
	
	return InFloat;
}

int UBlockoutLibrary_MathFunctions::IntToModuleInt(int InInt, int Module)
{
	if (Module > 0)
	{
		return InInt / Module * Module;
	}
	return InInt;
}

FVector UBlockoutLibrary_MathFunctions::VectorToModuleVector(FVector InVector, FVector Module)
{
	FVector OutVector;
	OutVector.X = FloatToModuleFloat(InVector.X, Module.X);
	OutVector.Y = FloatToModuleFloat(InVector.Y, Module.Y);
	OutVector.Z = FloatToModuleFloat(InVector.Z, Module.Z);
	return OutVector;
}

FVector2D UBlockoutLibrary_MathFunctions::Vector2DToModuleVector2D(FVector2D InVector2D, FVector2D Module)
{
	FVector2D OutVector2D;
	OutVector2D.X = FloatToModuleFloat(InVector2D.X, Module.X);
	OutVector2D.Y = FloatToModuleFloat(InVector2D.Y, Module.Y);
	return OutVector2D;
}

FVector4 UBlockoutLibrary_MathFunctions::Vector4ToModuleVector4(FVector4 InVector4, FVector4 Module)
{
	FVector4 OutVector4;
	OutVector4.X = FloatToModuleFloat(InVector4.X, Module.X);
	OutVector4.Y = FloatToModuleFloat(InVector4.Y, Module.Y);
	OutVector4.Z = FloatToModuleFloat(InVector4.Z, Module.Z);
	OutVector4.W = FloatToModuleFloat(InVector4.W, Module.W);
	return OutVector4;
}

FIntVector UBlockoutLibrary_MathFunctions::IntVectorToModuleIntVector(FIntVector InIntVector, FIntVector Module)
{
	FIntVector OutIntVector;
	OutIntVector.X = IntToModuleInt(InIntVector.X, Module.X);
	OutIntVector.Y = IntToModuleInt(InIntVector.Y, Module.Y);
	OutIntVector.Z = IntToModuleInt(InIntVector.Z, Module.Z);
	return OutIntVector;
}

FRotator UBlockoutLibrary_MathFunctions::GetRotFromTwoDir(
	FVector StartDir,
	FVector TargetDir
)
{
	StartDir = StartDir.GetSafeNormal(1e-4);
	TargetDir = TargetDir.GetSafeNormal(1e-4);

	FQuat StartQuat = FRotationMatrix::MakeFromX(StartDir).ToQuat().Inverse();
	FQuat TargetQuat = FRotationMatrix::MakeFromX(TargetDir).ToQuat();

	FRotator OutRotation = (StartQuat * TargetQuat).Rotator();
	return OutRotation;
}

FRotator UBlockoutLibrary_MathFunctions::GetLookAtRotator(
	FVector InLocation,
	FVector TargetLocation,
	FRotator InRotator
)
{
	FRotator LookAtRotator = UKismetMathLibrary::FindLookAtRotation(InLocation, TargetLocation);

	return FRotator(FQuat(InRotator) * FQuat(LookAtRotator));
}

bool UBlockoutLibrary_MathFunctions::LineIntersection(
	FVector2D LineAStart,
	FVector2D LineADir,
	float LineAMaxLength,
	FVector2D LineBStart,
	FVector2D LineBDir,
	float LineBMaxLength,
	FVector2D& IntersectPoint)
{
	IntersectPoint = FVector2D(0.0f, 0.0f);

	LineADir = LineADir.GetSafeNormal(1e-4);
	LineBDir = LineBDir.GetSafeNormal(1e-4);

	FVector2D A = LineAStart;
	FVector2D C = LineBStart;
	FVector2D B = LineAStart + LineADir * LineAMaxLength;
	FVector2D D = LineBStart + LineBDir * LineBMaxLength;

	float AreaABC = (A.X - C.X) * (B.Y - C.Y) - (A.Y - C.Y) * (B.X - C.X);
	float AreaABD = (A.X - D.X) * (B.Y - D.Y) - (A.Y - D.Y) * (B.X - D.X);

	if (AreaABC * AreaABD > 0.0f)
	{
		return false;
	}

	float AreaCDA = (C.X - A.X) * (D.Y - A.Y) - (C.Y - A.Y) * (D.X - A.X);
	float AreaCDB = AreaCDA + AreaABC - AreaABD;

	if (AreaCDA * AreaCDB >= 0.0f)
	{
		return false;
	}

	float IntersectT = AreaCDA / (AreaABD - AreaABC);

	IntersectPoint.X = IntersectT * (B.X - A.X) + A.X;
	IntersectPoint.Y = IntersectT * (B.Y - A.Y) + A.Y;
	return true;
}

void UBlockoutLibrary_MathFunctions::AnalysePointsFromEllipse(float Angle, float StartAngle, int Sections, float XRadius, float YRadius, bool bReversed, TArray<FVector>& Positions, TArray<FVector>& Tangents)
{
	Positions.Reset();
	Tangents.Reset();
	float SegmentRadiance = FMath::DegreesToRadians(Angle/float(Sections));
	SegmentRadiance *= bReversed?-1:1;
	float StartRadiance = FMath::DegreesToRadians(StartAngle);

	for (int i=0; i<=Sections; i++)
	{
		float CurRadiance = SegmentRadiance * float(i) + StartRadiance;
		// if (bReversed)
		// {
		// 	CurRadiance = 2*PI - CurRadiance;
		// }
		FVector CurPosition = FVector(FMath::Sin(CurRadiance)*XRadius, FMath::Cos(CurRadiance)*YRadius, 0.0f);
		FVector CurTangent = FVector(FMath::Cos(CurRadiance)*XRadius, -FMath::Sin(CurRadiance)*YRadius, 0.0f);
		Positions.Add(CurPosition);
		Tangents.Add(CurTangent);
	}
	// if (bReversed)
	// {
	// 	Algo::Reverse(Positions);
	// 	Algo::Reverse(Tangents);
	// }
}

FBox UBlockoutLibrary_MathFunctions :: CalAABB(TArray<UStaticMeshComponent*> StaticMeshComps, FTransform Transform=FTransform::Identity)
{
	// 初始化包围盒的最小和最大值
	FVector3f Min = (FVector3f)FVector::ZeroVector;
	FVector3f Max = (FVector3f)FVector::ZeroVector;

	int Index = 0;

	for (UStaticMeshComponent* MeshComp : StaticMeshComps)
	{
		// 获取StaticMesh
		UStaticMesh* StaticMesh = MeshComp->GetStaticMesh();

		if (IsValid(StaticMesh))
		{
			// 获取StaticMesh的顶点缓冲区
#if ENGINE_MAJOR_VERSION >= 5
			FPositionVertexBuffer* VertexBuffer = &StaticMesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
#else
			FPositionVertexBuffer* VertexBuffer = &StaticMesh->RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
#endif
			
			
			FTransform3f CompTransform = (FTransform3f)MeshComp->GetRelativeTransform();

			USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>(MeshComp);

			// 遍历所有顶点
			for (uint32 i = 0; i < VertexBuffer->GetNumVertices(); ++i)
			{
				// 获取顶点并将其转换坐标空间
				FVector3f StaticMeshVertex = VertexBuffer->VertexPosition(i);
				if (IsValid(SplineMeshComp))
				{
					const float VertexPositionAlongSpline = StaticMeshVertex[SplineMeshComp->ForwardAxis];
					const FTransform3f StaticMeshToSplineMeshTransform = static_cast<FTransform3f>(SplineMeshComp->CalcSliceTransform(VertexPositionAlongSpline));
					StaticMeshVertex[SplineMeshComp->ForwardAxis] = 0.0f;
					auto TransformedPos = StaticMeshToSplineMeshTransform.TransformPosition(StaticMeshVertex);
					StaticMeshVertex = FVector3f(TransformedPos.X, TransformedPos.Y, TransformedPos.Z);
				}
				FVector3f Vertex = ((FTransform3f)Transform).TransformPosition(CompTransform.TransformPosition(StaticMeshVertex));

				if (Index == 0) {
					Min = Vertex;
					Max = Vertex;
				}else
				{
					// 更新包围盒的最小和最大值
					Min.X = FMath::Min(Min.X, Vertex.X);
					Min.Y = FMath::Min(Min.Y, Vertex.Y);
					Min.Z = FMath::Min(Min.Z, Vertex.Z);
	
					Max.X = FMath::Max(Max.X, Vertex.X);
					Max.Y = FMath::Max(Max.Y, Vertex.Y);
					Max.Z = FMath::Max(Max.Z, Vertex.Z);
				}

				Index ++;
			}
		}
	}
	// 创建并返回新的包围盒
	return FBox(Min, Max);
}

FBox UBlockoutLibrary_MathFunctions::CalMeshAABB(UDynamicMesh* InMesh, FTransform Transform)
{
	if (!UBlockoutLibrary_BasicFunctions::IsDynamicMeshValid(InMesh)){
		return FBox();
	}
	
	float MaxX = TNumericLimits<float>::Min();
	float MaxY = TNumericLimits<float>::Min();
	float MaxZ = TNumericLimits<float>::Min();
	float MinX = TNumericLimits<float>::Max();
	float MinY = TNumericLimits<float>::Max();
	float MinZ = TNumericLimits<float>::Max();

	InMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		for (const FVector3d& VertexPos : ReadMesh.VerticesItr())
		{
			FVector Transformed_Pos = Transform.TransformPosition(FVector(VertexPos.X, VertexPos.Y, VertexPos.Z));
			MaxX = FMath::Max(Transformed_Pos.X, MaxX);
			MaxY = FMath::Max(Transformed_Pos.Y, MaxY);
			MaxZ = FMath::Max(Transformed_Pos.Z, MaxZ);

			MinX = FMath::Min(Transformed_Pos.X, MinX);
			MinY = FMath::Min(Transformed_Pos.Y, MinY);
			MinZ = FMath::Min(Transformed_Pos.Z, MinZ);
		}
	});

	const FVector& Min = FVector(MinX, MinY, MinZ);
	const FVector& Max = FVector(MaxX, MaxY, MaxZ);
	
	return FBox(Min, Max);
}
