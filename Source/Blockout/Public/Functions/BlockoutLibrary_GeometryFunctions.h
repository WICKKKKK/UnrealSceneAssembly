#pragma once

#include "BlockoutEnum.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UDynamicMesh.h"

#include "BlockoutLibrary_GeometryFunctions.generated.h"

class UDynamicMeshPool;
class USplineComponent;

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_GeometryFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Geometry")
	static UPARAM(DisplayName="Target Mesh") UDynamicMesh* CleanDegenerateTris(UDynamicMesh* TargetMesh, float SafeNormalTolerance = 1e-4f, bool DebugLog = true, bool DeleteDegenerates = true);

	UFUNCTION(BlueprintCallable, Category="Blockout|Geometry")
	static UPARAM(DisplayName="Target Mesh") UDynamicMesh* GeneratePlanarMesh(UDynamicMesh* TargetMesh, USplineComponent* Spline, int32 InterpolateNum);

	UFUNCTION(BlueprintCallable, Category="Blockout|Geometry")
	static UPARAM(DisplayName="Target Mesh") UDynamicMesh* GenerateSplineMesh(UDynamicMesh* TargetMesh, USplineComponent* Spline, float Width, int32 InterpolateNum, bool bReverse, bool bCenterize);

	UFUNCTION(BlueprintCallable, Category="Blockout|Geometry")
	static UPARAM(DisplayName="Target Mesh") UDynamicMesh* GenerateRoofMesh(UDynamicMesh* TargetMesh, UDynamicMeshPool* MeshPool, float RoofHeight, float RotateRidgeAngle, FVector& RidgeDir);
};
