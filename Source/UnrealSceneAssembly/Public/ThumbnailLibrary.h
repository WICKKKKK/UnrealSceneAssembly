#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThumbnailLibrary.generated.h"

UCLASS()
class UNREALSCENEASSEMBLY_API UThumbnailLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Export Asset Thumbnail"))
	static bool ExportAssetThumbnail(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution = 512);
};
