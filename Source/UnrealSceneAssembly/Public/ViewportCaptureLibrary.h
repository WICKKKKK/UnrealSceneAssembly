#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ViewportCaptureLibrary.generated.h"

UCLASS()
class UNREALSCENEASSEMBLY_API UViewportCaptureLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scene Assembly", meta = (DisplayName = "Capture Active Viewport"))
	static bool CaptureActiveViewport(const FString& OutPngPath, int32 Width = 0, int32 Height = 0);
};
