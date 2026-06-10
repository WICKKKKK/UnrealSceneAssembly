#pragma once

#include "BlockoutEnum.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BlockoutLibrary_SplineFunctions.generated.h"

class ABlockoutBaseDynamicMeshActor;
class USplineComponent;

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_SplineFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void SetSplinePointType(USplineComponent* Spline, EBlockoutSplinePointType SplinePointType = EBlockoutSplinePointType::Linear);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void ShowSplineID(USplineComponent* Spline, ABlockoutBaseDynamicMeshActor* BlockoutActor, bool bIsShow);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void GetSplinePointNeighbours(int32 PointNum, bool bIsClosed, int32 Index, int32 InterpolateNum, int32& OutPrevIndex, int32& OutNextIndex, int32& OutNeighbourCount);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void PolylineOffset(USplineComponent* Spline, float OffsetDistance, bool bReverse, int32 InterpolateNum, TArray<FVector>& OutSplinePoints);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void PolylineOffsetBySplinePoints(TArray<FVector> InSplinePoints, bool bIsClosed, float OffsetDistance, bool bReverse, TArray<FVector>& OutOffsetPoints);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void GetSplinePointLocationWithInterp(USplineComponent* Spline, int32 InterpolateNum, TArray<int32>& OutSplineIndexeList, TArray<FVector>& OutSplinePointList, TArray<FVector>& OutTangentList, FVector& OutSplineCenter);

	UFUNCTION(BlueprintCallable, Category="Blockout|Spline")
	static void AnalyseSplineWithInterp(USplineComponent* Spline, int32 InterpolateNum, TArray<int32>& OutSplineIndexeList, TArray<FVector>& OutSplinePointList, TArray<FVector>& OutTangentList, TArray<FVector>& OutOffsetDirList, FVector& OutSplineCenter);
};
