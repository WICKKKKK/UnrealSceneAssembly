#include "ViewportCaptureLibrary.h"

#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/UnrealMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneAssemblyViewportCapture, Log, All);

bool UViewportCaptureLibrary::CaptureActiveViewport(const FString& OutPngPath, int32 Width, int32 Height)
{
#if WITH_EDITOR
	if (OutPngPath.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("OutPngPath is empty."));
		return false;
	}

	if (!GEditor)
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("GEditor is unavailable."));
		return false;
	}

	FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("No active editor viewport is available."));
		return false;
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Active viewport has invalid size: %dx%d."), ViewportSize.X, ViewportSize.Y);
		return false;
	}

	TArray<FColor> Bitmap;
	Bitmap.SetNumUninitialized(ViewportSize.X * ViewportSize.Y);

	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	if (!Viewport->ReadPixels(Bitmap, ReadFlags, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y)))
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Failed to read active viewport pixels."));
		return false;
	}

	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	const int32 TargetWidth = Width > 0 ? FMath::Clamp(Width, 16, 8192) : ViewportSize.X;
	const int32 TargetHeight = Height > 0 ? FMath::Clamp(Height, 16, 8192) : ViewportSize.Y;

	if (TargetWidth != ViewportSize.X || TargetHeight != ViewportSize.Y)
	{
		TArray<FColor> ResizedBitmap;
		FImageUtils::ImageResize(ViewportSize.X, ViewportSize.Y, Bitmap, TargetWidth, TargetHeight, ResizedBitmap, false);
		Bitmap = MoveTemp(ResizedBitmap);
	}

	if (Bitmap.Num() < TargetWidth * TargetHeight)
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Viewport bitmap has invalid data size: %d for %dx%d."), Bitmap.Num(), TargetWidth, TargetHeight);
		return false;
	}

	const FString OutputDirectory = FPaths::GetPath(OutPngPath);
	if (!OutputDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Failed to create screenshot directory: %s"), *OutputDirectory);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Failed to create PNG image wrapper."));
		return false;
	}

	const int64 RawDataSize = static_cast<int64>(Bitmap.Num()) * sizeof(FColor);
	if (!ImageWrapper->SetRaw(Bitmap.GetData(), RawDataSize, TargetWidth, TargetHeight, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Failed to encode viewport screenshot."));
		return false;
	}

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(100);
	if (CompressedData.Num() <= 0 || CompressedData.Num() > MAX_int32)
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Invalid compressed PNG size: %lld."), static_cast<long long>(CompressedData.Num()));
		return false;
	}

	TArray<uint8> PngBytes;
	PngBytes.SetNumUninitialized(static_cast<int32>(CompressedData.Num()));
	FMemory::Memcpy(PngBytes.GetData(), CompressedData.GetData(), static_cast<SIZE_T>(CompressedData.Num()));

	if (!FFileHelper::SaveArrayToFile(PngBytes, *OutPngPath))
	{
		UE_LOG(LogSceneAssemblyViewportCapture, Warning, TEXT("Failed to save viewport screenshot: %s"), *OutPngPath);
		return false;
	}

	return true;
#else
	return false;
#endif
}
