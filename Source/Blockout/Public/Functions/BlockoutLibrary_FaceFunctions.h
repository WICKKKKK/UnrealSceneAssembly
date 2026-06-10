#pragma once

#include "CoreMinimal.h"
#include "BlockoutStruct.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BlockoutLibrary_FaceFunctions.generated.h"

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_FaceFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Box Faces") TArray<FBlockoutFace> GetBoxFaces(const FBox& Box, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Projected Point") FVector ProjectPointToFace(const FVector& InPoint, const FBlockoutFace& TargetFace, bool& bInside, float& ProjectDistance);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Projected Direction") FVector ProjectDirectionToFace(const FVector& InDirection, const FBlockoutFace& TargetFace);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Rotated Face") FBlockoutFace RotateFaceByOrigin(const FBlockoutFace& TargetFace, const FQuat& Rotation);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Be Parallel") bool AreFacesParallel(const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace, float AngleThreshold = 10.0f);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Projection Distance") float CalculateProjectionDistanceBetweenFaces(const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Align Normal Rotation") FQuat CalculateAlignFaceNormalRotation(const FVector& TargetDirection, const FBlockoutFace& OtherFace);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Be Found Axis") bool FindClosestFaceAxis(const FVector& TargetDirection, const FBlockoutFace& OtherFace, FVector& OutClosestAxis, FVector& OutProjectedDirection);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Be Succeed") bool CalculateMinEdgeDistanceBetweenFacesInDirection(const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace, const FVector& Direction, float& MinEdgeDistance, FVector& OutTargetAxis, FVector& OutOtherAxis);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Align Axis Rotation") FQuat CalculateAlignFaceAxisRotation(const FVector& TargetDirection, const FBlockoutFace& OtherFace);

	UFUNCTION(BlueprintCallable, Category="Blockout|Face")
	static UPARAM(DisplayName="Be Found Edge") bool CalculateMinEdgeDistanceBetweenFaces(const FBlockoutFace& TargetFace, const FBlockoutFace& OtherFace, float EdgeSnapThreshold, float& OutMinEdgeDistance, FVector& OutTargetEdgeCenter, FVector& OutOtherAxis);
};
