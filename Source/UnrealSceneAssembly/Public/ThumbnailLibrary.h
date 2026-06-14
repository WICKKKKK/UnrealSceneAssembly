#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThumbnailLibrary.generated.h"

USTRUCT(BlueprintType)
struct UNREALSCENEASSEMBLY_API FThumbnailCaptureOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "override_camera"))
	bool bOverrideCamera = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "orbit_pitch", EditCondition = "bOverrideCamera"))
	float OrbitPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "orbit_yaw", EditCondition = "bOverrideCamera"))
	float OrbitYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "orbit_zoom", EditCondition = "bOverrideCamera"))
	float OrbitZoom = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "fov_degrees", ClampMin = "1.0", ClampMax = "170.0"))
	float FovDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene Assembly", meta = (ScriptName = "resolution", ClampMin = "0", ClampMax = "2048"))
	int32 Resolution = 0;
};

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

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail With Options"))
	static bool ExportAssetThumbnailWithOptions(const FString& AssetObjectPath, const FString& OutPngPath, const FThumbnailCaptureOptions& CaptureOptions, int32 Resolution = 512, FLinearColor BackgroundColor = FLinearColor::Gray);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail With Camera"))
	static FThumbnailExportResult ExportAssetThumbnailWithCamera(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution = 512, FLinearColor BackgroundColor = FLinearColor::Gray);

	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail With Camera And Options"))
	static FThumbnailExportResult ExportAssetThumbnailWithCameraAndOptions(const FString& AssetObjectPath, const FString& OutPngPath, const FThumbnailCaptureOptions& CaptureOptions, int32 Resolution = 512, FLinearColor BackgroundColor = FLinearColor::Gray);
};
