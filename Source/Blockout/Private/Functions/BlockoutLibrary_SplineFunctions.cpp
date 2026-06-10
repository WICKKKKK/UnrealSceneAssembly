#include "Functions/BlockoutLibrary_SplineFunctions.h"
#include "Components/SplineComponent.h"
#include "BlockoutBaseDynamicMeshActor.h"
#include "Components/TextRenderComponent.h"


void UBlockoutLibrary_SplineFunctions::SetSplinePointType(
	USplineComponent* Spline,
	EBlockoutSplinePointType SplinePointType
)
{
	if (IsValid(Spline))
	{
		for (int i=0; i<Spline->GetNumberOfSplinePoints(); i++)
		{
			ESplinePointType::Type OriPointType = Spline->GetSplinePointType(i);
			if ((OriPointType == ESplinePointType::Type::Linear ||
				OriPointType == ESplinePointType::Type::Constant) && SplinePointType == EBlockoutSplinePointType::Curve)
			{
				Spline->SetSplinePointType(i, ESplinePointType::Type::Curve, false);
			}else if ((OriPointType == ESplinePointType::Type::Curve || OriPointType == ESplinePointType::Type::CurveClamped ||
					  OriPointType == ESplinePointType::Type::CurveCustomTangent) && SplinePointType == EBlockoutSplinePointType::Linear)
			{
				Spline->SetSplinePointType(i, ESplinePointType::Type::Linear, false);
			}
		}
		Spline->UpdateSpline();
	}
}

void UBlockoutLibrary_SplineFunctions::ShowSplineID(
	USplineComponent* Spline,
	ABlockoutBaseDynamicMeshActor* BlockoutActor,
	bool bIsShow)
{
	TArray<USceneComponent*> ChildArray;
	BlockoutActor->GetRootComponent()->GetChildrenComponents(true,ChildArray);
	for(const auto& Child : ChildArray)
	{
		if(Child->IsA(UTextRenderComponent::StaticClass()))
		{
			Child->DestroyComponent();
		}
	}

	if(!bIsShow)
	{
		return;
	}
	
	const int PointNum = Spline->GetNumberOfSplinePoints();
	for(int i=0; i<PointNum; i++)
	{
		const FVector Pos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		UTextRenderComponent* CurTextComp = NewObject<UTextRenderComponent>(BlockoutActor, NAME_None, EObjectFlags::RF_Transient);
		CurTextComp->RegisterComponent();
		CurTextComp->AttachToComponent(BlockoutActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		// CurTextComp->SetupAttachment(BlockoutActor->GetRootComponent());  好像只能 Attach 一次，因此动态 Attach 的时候最好不要用
		CurTextComp->SetRelativeLocation(Pos);
		// BlockoutActor->AddInstanceComponent(CurTextComp);
		CurTextComp->SetText(FText::AsNumber(i));
	}
}


void UBlockoutLibrary_SplineFunctions::GetSplinePointLocationWithInterp(
	USplineComponent* Spline,
	int InterpolateNum,
	TArray<int>& OutSplineIndexeList,
	TArray<FVector>& OutSplinePointList,
	TArray<FVector>& OutTangentList,
	FVector& OutSplineCenter
	)
{
	OutSplineCenter = FVector(0.0f, 0.0f, 0.0f);
	int PointNum = Spline->GetNumberOfSplinePoints();
	bool bIsClosed = Spline->IsClosedLoop();
	
	for (int i=0; i<PointNum; ++i)
	{
		int PrevIndex;
		int NextIndex;
		int NeighbourCount;
		GetSplinePointNeighbours(PointNum, bIsClosed, i, 0, PrevIndex, NextIndex, NeighbourCount);
		
		FVector CurPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Type::Local);
		FVector CurTangent;

		if (Spline->GetSplinePointType(i) == ESplinePointType::Type::Linear)
		{
			if (NeighbourCount == 1)
			{
				if (NextIndex >= 0)
				{
					FVector NextPos = Spline->GetLocationAtSplinePoint(NextIndex, ESplineCoordinateSpace::Type::Local);
					CurTangent = (NextPos - CurPos).GetSafeNormal(1e-4);
				}else if (PrevIndex >= 0)
				{
					FVector PrevPos = Spline->GetLocationAtSplinePoint(PrevIndex, ESplineCoordinateSpace::Type::Local);
					CurTangent = (CurPos - PrevPos).GetSafeNormal(1e-4);
				}
			}else
			{
				FVector PrevPos = Spline->GetLocationAtSplinePoint(PrevIndex, ESplineCoordinateSpace::Type::Local);
				FVector NextPos = Spline->GetLocationAtSplinePoint(NextIndex, ESplineCoordinateSpace::Type::Local);
				CurTangent = ((NextPos - CurPos).GetSafeNormal(1e-4) + (CurPos - PrevPos).GetSafeNormal(1e-4)).GetSafeNormal(1e-4);
			}
		}else
		{
			CurTangent = (Spline->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Type::Local) +
						Spline->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Type::Local)).GetSafeNormal(1e-4);
		}
		
		float SegTime;
		float CurTime;
		if (bIsClosed)
		{
			SegTime = 1.0f / float(PointNum);
			CurTime = float(i) / float(PointNum);
		}else
		{
			SegTime = 1.0f / float(PointNum-1);
			CurTime = float(i) / float(PointNum-1);
		}
		
		OutSplineIndexeList.Add(i);
		OutSplinePointList.Add(CurPos);
		OutTangentList.Add(CurTangent);
		OutSplineCenter += CurPos;
		
		if (NextIndex >= 0 && InterpolateNum >= 1)
		{
			float InterpSegTime = SegTime / float(InterpolateNum+1);

			for (int j=1; j<InterpolateNum+1; ++j)
			{
				float CurInterpTime = float(j) * InterpSegTime + CurTime;
				FVector CurInterpPos = Spline->GetLocationAtTime(CurInterpTime, ESplineCoordinateSpace::Type::Local, false);
				FVector CurInterpTangent = Spline->GetTangentAtTime(CurInterpTime, ESplineCoordinateSpace::Type::Local, false);
				OutSplinePointList.Add(CurInterpPos);
				OutTangentList.Add(CurInterpTangent);
			}
		}
	}

	OutSplineCenter /= PointNum;
}

void UBlockoutLibrary_SplineFunctions::GetSplinePointNeighbours(
	int PointNum,
	bool bIsClosed,
	int Index,
	int InterpolateNum,
	int& OutPrevIndex,
	int& OutNextIndex,
	int& OutNeighbourCount
	)
{
	OutPrevIndex = -1;
	OutNextIndex = -1;
	OutNeighbourCount = 0;
	
	PointNum += (bIsClosed ? PointNum : PointNum - 1) * InterpolateNum;
	if (Index >= PointNum || PointNum <= 1 || Index < 0)
	{
		return;
	}
	
	if (PointNum == 2)
	{
		if (bIsClosed)
		{
			if (Index == 0)
			{
				OutNextIndex = 1;
				OutPrevIndex = 1;
			}else
			{
				OutNextIndex = 0;
				OutPrevIndex = 0;
			}
		}else
		{
			if (Index == 0)
			{
				OutNextIndex = 1;
			}else
			{
				OutPrevIndex = 0;
			}
		}
	}else
	{
		if (bIsClosed)
		{
			if (Index == 0)
			{
				OutNextIndex = 1;
				OutPrevIndex = PointNum - 1;
			}else
			{
				if (Index == PointNum-1)
				{
					OutNextIndex = 0;
					OutPrevIndex = Index - 1;
				}else
				{
					OutNextIndex = Index + 1;
					OutPrevIndex = Index - 1;
				}
			}
		}else
		{
			if (Index == 0)
			{
				OutNextIndex = 1;
			}else
			{
				if (Index == PointNum-1)
				{
					OutPrevIndex = Index - 1;
				}else
				{
					OutNextIndex = Index + 1;
					OutPrevIndex = Index - 1;
				}
			}
		}
	}
	OutNeighbourCount += OutPrevIndex>=0 ? 1 : 0;
	OutNeighbourCount += OutNextIndex>=0 ? 1 : 0;
}


void UBlockoutLibrary_SplineFunctions::PolylineOffset(
	USplineComponent* Spline,
	float OffsetDistance,
	bool bReverse,
	int InterpolateNum,
	TArray<FVector>& OutSplinePoints)
{
	TArray<int> SplineIndexes;
	TArray<FVector> BeforeOffsetSplinePoints;
	TArray<FVector> OutTangentList;
	FVector SplineCenter;
	GetSplinePointLocationWithInterp(Spline, InterpolateNum, SplineIndexes, BeforeOffsetSplinePoints,
		OutTangentList, SplineCenter);

	int PointNum = BeforeOffsetSplinePoints.Num();
	bool bIsClosed = Spline->IsClosedLoop();
	
	if ((bIsClosed && PointNum > 2) || (!bIsClosed && PointNum > 1))
	{
		for (int i=0; i<PointNum; ++i)
		{
			FVector CurPos = BeforeOffsetSplinePoints[i];
			
			int PrevIndex;
			int NextIndex;
			int NeighbourCount;
			GetSplinePointNeighbours(PointNum, bIsClosed, i, InterpolateNum, PrevIndex, NextIndex, NeighbourCount);

			FVector OffsetDir;
			
			if (NeighbourCount == 1)
			{
				int NeiIndex = FMath::Max(PrevIndex, NextIndex);
				FVector NeiDir = FVector(BeforeOffsetSplinePoints[NeiIndex].X-CurPos.X,
										 BeforeOffsetSplinePoints[NeiIndex].Y-CurPos.Y,
										 0.0f).GetSafeNormal(1e-4);
				FVector NeiTangent;
				if (NeiIndex < i)
				{
					NeiTangent = FVector::CrossProduct(NeiDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);
				}else
				{
					NeiTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NeiDir).GetSafeNormal(1e-4);
				}
				OffsetDir = NeiTangent;
				OffsetDir.Z = 0.0f;
			}else
			{
				FVector NextDir = FVector(BeforeOffsetSplinePoints[NextIndex].X-CurPos.X,
										  BeforeOffsetSplinePoints[NextIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector PrevDir = FVector(BeforeOffsetSplinePoints[PrevIndex].X-CurPos.X,
										  BeforeOffsetSplinePoints[PrevIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector NextTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NextDir).GetSafeNormal(1e-4);
				FVector PrevTangent = FVector::CrossProduct(PrevDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);

				OffsetDir = (NextTangent + PrevTangent).GetSafeNormal(1e-4);
				OffsetDir *= FMath::Pow(2.0f/(FVector::DotProduct(NextTangent, PrevTangent) + 1.0f), 0.5f);
				OffsetDir.Z = 0.0f;
			}

			if (bReverse)
			{
				OffsetDir *= -1.0f;
			}
			FVector OffsetPos = OffsetDir * OffsetDistance + CurPos;
			
			OutSplinePoints.Add(OffsetPos);
		}
	}
}

void UBlockoutLibrary_SplineFunctions::PolylineOffsetBySplinePoints(
	TArray<FVector> InSplinePoints,
	bool bIsClosed,
	float OffsetDistance,
	bool bReverse,
	TArray<FVector>& OutOffsetPoints
	)
{
	int PointNum = InSplinePoints.Num();
	
	if ((bIsClosed && PointNum > 2) || (!bIsClosed && PointNum > 1))
	{
		for (int i=0; i<PointNum; ++i)
		{
			FVector CurPos = InSplinePoints[i];
			
			int PrevIndex;
			int NextIndex;
			int NeighbourCount;
			GetSplinePointNeighbours(PointNum, bIsClosed, i, 0,PrevIndex, NextIndex, NeighbourCount);

			FVector OffsetDir;
			
			if (NeighbourCount == 1)
			{
				int NeiIndex = FMath::Max(PrevIndex, NextIndex);
				FVector NeiDir = FVector(InSplinePoints[NeiIndex].X-CurPos.X,
										 InSplinePoints[NeiIndex].Y-CurPos.Y,
										 0.0f).GetSafeNormal(1e-4);
				FVector NeiTangent;
				if (NeiIndex < i)
				{
					NeiTangent = FVector::CrossProduct(NeiDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);
				}else
				{
					NeiTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NeiDir).GetSafeNormal(1e-4);
				}
				OffsetDir = NeiTangent;
				OffsetDir.Z = 0.0f;
			}else
			{
				FVector NextDir = FVector(InSplinePoints[NextIndex].X-CurPos.X,
										  InSplinePoints[NextIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector PrevDir = FVector(InSplinePoints[PrevIndex].X-CurPos.X,
										  InSplinePoints[PrevIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector NextTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NextDir).GetSafeNormal(1e-4);
				FVector PrevTangent = FVector::CrossProduct(PrevDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);

				OffsetDir = (NextTangent + PrevTangent).GetSafeNormal(1e-4);
				OffsetDir *= FMath::Pow(2.0f/(FVector::DotProduct(NextTangent, PrevTangent) + 1.0f), 0.5f);
				OffsetDir.Z = 0.0f;
			}

			if (bReverse)
			{
				OffsetDir *= -1.0f;
			}
			FVector OffsetPos = OffsetDir * OffsetDistance + CurPos;
			
			OutOffsetPoints.Add(OffsetPos);
		}
	}
}

void UBlockoutLibrary_SplineFunctions::AnalyseSplineWithInterp(
	USplineComponent* Spline,
	int InterpolateNum,
	TArray<int>& OutSplineIndexeList,
	TArray<FVector>& OutSplinePointList,
	TArray<FVector>& OutTangentList,
	TArray<FVector>& OutOffsetDirList,
	FVector& OutSplineCenter)
{
	GetSplinePointLocationWithInterp(Spline, InterpolateNum, OutSplineIndexeList, OutSplinePointList, OutTangentList, OutSplineCenter);

	int PointNum = OutSplinePointList.Num();
	bool bIsClosed = Spline->IsClosedLoop();
	
	if ((bIsClosed && PointNum > 2) || (!bIsClosed && PointNum > 1))
	{
		int CurIndex = 0;
		for (FVector CurPos : OutSplinePointList)
		{
			int PrevIndex;
			int NextIndex;
			int NeighbourCount;
			GetSplinePointNeighbours(PointNum, bIsClosed, CurIndex, InterpolateNum, PrevIndex, NextIndex, NeighbourCount);

			FVector OffsetDir;
			
			if (NeighbourCount == 1)
			{
				int NeiIndex = FMath::Max(PrevIndex, NextIndex);
				FVector NeiDir = FVector(OutSplinePointList[NeiIndex].X-CurPos.X,
										 OutSplinePointList[NeiIndex].Y-CurPos.Y,
										 0.0f).GetSafeNormal(1e-4);
				FVector NeiTangent;
				if (NeiIndex < CurIndex)
				{
					NeiTangent = FVector::CrossProduct(NeiDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);
				}else
				{
					NeiTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NeiDir).GetSafeNormal(1e-4);
				}
				OffsetDir = NeiTangent;
				OffsetDir.Z = 0.0f;
			}else
			{
				FVector NextDir = FVector(OutSplinePointList[NextIndex].X-CurPos.X,
										  OutSplinePointList[NextIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector PrevDir = FVector(OutSplinePointList[PrevIndex].X-CurPos.X,
										  OutSplinePointList[PrevIndex].Y-CurPos.Y,
										  0.0f).GetSafeNormal(1e-4);

				FVector NextTangent = FVector::CrossProduct(FVector(0.0f, 0.0f, 1.0f), NextDir).GetSafeNormal(1e-4);
				FVector PrevTangent = FVector::CrossProduct(PrevDir, FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal(1e-4);

				OffsetDir = (NextTangent + PrevTangent).GetSafeNormal(1e-4);
				OffsetDir *= FMath::Pow(2.0f/(FVector::DotProduct(NextTangent, PrevTangent) + 1.0f), 0.5f);
				OffsetDir.Z = 0.0f;
			}
			
			// 如果想要计算偏移后的点位置, 用 OffsetPos + CurPos;
			// 如果想要反转偏移方向, 用 OffsetPos *= -1.0f;
			// 如果想要控制偏移距离, 用 OffsetPos *= OffsetDistance;
			OutOffsetDirList.Add(OffsetDir);
			++CurIndex;
		}
	}
}
