#pragma once

#include "CoreMinimal.h"
#include "SceneCaptureTypes.generated.h"

USTRUCT(BlueprintType)
struct FSceneCaptureCameraInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	float FovHorizontal = 90.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Capture|Camera")
	float AspectRatio = 1.0f;
};
