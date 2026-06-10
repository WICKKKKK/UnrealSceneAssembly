#pragma once

#include "BlockoutStruct.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UDynamicMesh.h"

#include "BlockoutLibrary_MathFunctions.generated.h"

UCLASS()
class BLOCKOUT_API UBlockoutLibrary_MathFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutFloat") float BlockoutFloatToFloat(FBlockoutFloat InFloat, float Module = 0.01f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutInt") int32 BlockoutIntToInt(FBlockoutInt InInt, int32 Module = 1);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector") FVector BlockoutVectorToVector(FBlockoutFVector InVector, FVector Module = FVector(0.01f));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector2D") FVector2D BlockoutVector2DToVector2D(FBlockoutFVector2D InVector2D, FVector2D Module = FVector2D(0.01f, 0.01f));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector4") FVector4 BlockoutVector4ToVector4(FBlockoutFVector4 InVector4, FVector4 Module);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutIntVector") FIntVector BlockoutIntVectorToIntVector(FBlockoutFIntVector InIntVector, FIntVector Module);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static void BlockoutIntVector2DToIntVector2D(FBlockoutFIntVector2D InIntVector2D, int32& OutX, int32& OutY, int32 ModuleX = 1, int32 ModuleY = 1);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutFloat") float FloatToModuleFloat(float InFloat, float Module = 0.01f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutInt") int32 IntToModuleInt(int32 InInt, int32 Module = 1);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector") FVector VectorToModuleVector(FVector InVector, FVector Module = FVector(0.01f));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector2D") FVector2D Vector2DToModuleVector2D(FVector2D InVector2D, FVector2D Module = FVector2D(0.01f, 0.01f));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutVector4") FVector4 Vector4ToModuleVector4(FVector4 InVector4, FVector4 Module);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutIntVector") FIntVector IntVectorToModuleIntVector(FIntVector InIntVector, FIntVector Module);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="Rotation") FRotator GetRotFromTwoDir(FVector StartDir, FVector TargetDir);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Blockout|Math")
	static UPARAM(DisplayName="OutRotation") FRotator GetLookAtRotator(FVector InLocation, FVector TargetLocation, FRotator InRotator);

	UFUNCTION(BlueprintCallable, Category="Blockout|Math")
	static UPARAM(DisplayName="Is Intersect") bool LineIntersection(FVector2D LineAStart, FVector2D LineADir, float LineAMaxLength, FVector2D LineBStart, FVector2D LineBDir, float LineBMaxLength, FVector2D& IntersectPoint);

	UFUNCTION(BlueprintCallable, Category="Blockout|Math")
	static void AnalysePointsFromEllipse(float Angle, float StartAngle, int32 Sections, float XRadius, float YRadius, bool bReversed, TArray<FVector>& Positions, TArray<FVector>& Tangents);

	UFUNCTION(BlueprintCallable, Category="Blockout|Math")
	static UPARAM(DisplayName="OutAABB") FBox CalAABB(TArray<UStaticMeshComponent*> StaticMeshComps, FTransform Transform);

	UFUNCTION(BlueprintCallable, Category="Blockout|Math")
	static UPARAM(DisplayName="OutAABB") FBox CalMeshAABB(UDynamicMesh* InMesh, FTransform Transform);
};
