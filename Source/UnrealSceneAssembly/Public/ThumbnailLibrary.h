#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThumbnailLibrary.generated.h"

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FThumbnailExportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scene Assembly", meta = (ScriptName = "success"))
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scene Assembly", meta = (ScriptName = "camera_rotation"))
	FRotator CameraRotation = FRotator::ZeroRotator;
};

UCLASS()
class UNREALSCENEASSEMBLY_API UThumbnailLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail"))
	static bool ExportAssetThumbnail(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution = 512, FLinearColor BackgroundColor = FLinearColor::Gray);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail With Camera"))
	static FThumbnailExportResult ExportAssetThumbnailWithCamera(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution = 512, FLinearColor BackgroundColor = FLinearColor::Gray);
};
