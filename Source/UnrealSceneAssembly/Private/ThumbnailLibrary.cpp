#include "ThumbnailLibrary.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "HAL/UnrealMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "RenderingThread.h"
#include "StaticMeshCompiler.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneAssemblyThumbnail, Log, All);

bool UThumbnailLibrary::ExportAssetThumbnail(const FString& AssetObjectPath, const FString& OutPngPath, int32 Resolution)
{
#if WITH_EDITOR
	if (AssetObjectPath.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("AssetObjectPath is empty."));
		return false;
	}

	if (OutPngPath.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("OutPngPath is empty for %s."), *AssetObjectPath);
		return false;
	}

	FSoftObjectPath SoftObjectPath(AssetObjectPath);
	UObject* Asset = SoftObjectPath.TryLoad();
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetObjectPath);
	}

	if (!Asset)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to load asset: %s"), *AssetObjectPath);
		return false;
	}

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		if (StaticMesh->IsCompiling())
		{
			TArray<UStaticMesh*> StaticMeshesToCompile;
			StaticMeshesToCompile.Add(StaticMesh);
			FStaticMeshCompilingManager::Get().FinishCompilation(StaticMeshesToCompile);
		}

		StaticMesh->WaitForStreaming();
	}

	FlushRenderingCommands();

	const uint32 ThumbnailSize = static_cast<uint32>(FMath::Clamp(Resolution, 64, 2048));
	FObjectThumbnail Thumbnail;
	ThumbnailTools::RenderThumbnail(
		Asset,
		ThumbnailSize,
		ThumbnailSize,
		ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
		nullptr,
		&Thumbnail);

	if (Thumbnail.IsEmpty())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Generated thumbnail is empty: %s"), *AssetObjectPath);
		return false;
	}

	const int32 Width = Thumbnail.GetImageWidth();
	const int32 Height = Thumbnail.GetImageHeight();
	const TArray<uint8>& RawData = Thumbnail.GetUncompressedImageData();
	if (Width <= 0 || Height <= 0 || RawData.Num() < Width * Height * 4)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Invalid thumbnail data for %s (%dx%d, bytes=%d)."), *AssetObjectPath, Width, Height, RawData.Num());
		return false;
	}

	const FString OutputDirectory = FPaths::GetPath(OutPngPath);
	if (!OutputDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create thumbnail directory: %s"), *OutputDirectory);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to create PNG image wrapper."));
		return false;
	}

	if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to encode thumbnail data for %s."), *AssetObjectPath);
		return false;
	}

	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(100);
	if (CompressedData.Num() <= 0 || CompressedData.Num() > MAX_int32)
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Invalid compressed PNG size for %s."), *AssetObjectPath);
		return false;
	}

	TArray<uint8> PngBytes;
	PngBytes.SetNumUninitialized(static_cast<int32>(CompressedData.Num()));
	FMemory::Memcpy(PngBytes.GetData(), CompressedData.GetData(), static_cast<SIZE_T>(CompressedData.Num()));

	if (!FFileHelper::SaveArrayToFile(PngBytes, *OutPngPath))
	{
		UE_LOG(LogSceneAssemblyThumbnail, Warning, TEXT("Failed to save thumbnail: %s"), *OutPngPath);
		return false;
	}

	return true;
#else
	return false;
#endif
}
