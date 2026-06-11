#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "SceneCaptureLibrary.generated.h"

UCLASS()
class SCENECAPTURE_API USceneCaptureLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture Scene From Active Viewport"))
	static bool CaptureSceneFromActiveViewport(const FString& OutputDir, const FString& BaseName, int32 Width = 0, int32 Height = 0);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture Scene Fast From Active Viewport"))
	static bool CaptureSceneFastFromActiveViewport(const FString& OutputDir, const FString& BaseName, int32 Width = 0, int32 Height = 0);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture ID Map From Active Viewport"))
	static bool CaptureIdMapFromActiveViewport(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, int32 Width = 0, int32 Height = 0, bool bOnlyVisibleTargets = true);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture Scene And ID Map From Active Viewport"))
	static bool CaptureSceneAndIdMap(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, int32 Width = 0, int32 Height = 0, bool bOnlyVisibleTargets = true);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture Scene And ID Map From Actors"))
	static bool CaptureSceneAndIdMapFromActors(const TArray<AActor*>& TargetActors, const FString& OutputDir, const FString& BaseName, int32 Width = 0, int32 Height = 0, bool bOnlyVisibleTargets = true);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Capture Scene And ID Map Fast From Active Viewport"))
	static bool CaptureSceneAndIdMapFast(const FString& OutputDir, const FString& BaseName, const TArray<TSubclassOf<AActor>>& TargetClasses, int32 Width = 0, int32 Height = 0, bool bOnlyVisibleTargets = true);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Crop Image Region To Base64"))
	static void CropImageRegionToBase64(const FString& SourceImagePath, int32 RefWidth, int32 RefHeight, int32 XMin, int32 YMin, int32 XMax, int32 YMax, int32 ExpandPixels, FString& OutDataUri);

	UFUNCTION(BlueprintCallable, Category = "Scene Capture", meta = (DisplayName = "Crop Image Region To File"))
	static bool CropImageRegionToFile(const FString& SourceImagePath, int32 RefWidth, int32 RefHeight, int32 XMin, int32 YMin, int32 XMax, int32 YMax, int32 ExpandPixels, const FString& OutPngPath);
};
